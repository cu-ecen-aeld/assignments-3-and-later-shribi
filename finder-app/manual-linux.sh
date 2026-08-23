#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

if [ ! -d "${OUTDIR}" ]; then
    mkdir -p ${OUTDIR}
fi

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- mrproper
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- defconfig
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- -j6
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- dtbs
fi

echo "Adding the Image in outdir"
cp "${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image" "${OUTDIR}"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

sudo rm -rf *.cpio.gz

mkdir -p ${OUTDIR}/rootfs
cd ${OUTDIR}/rootfs
mkdir -p bin dev etc home lib lib64 proc sys sbin tmp usr var
mkdir -p usr/bin usr/sbin usr/lib
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
    CLONE_SUCCESS=0
    for i in 1 2 3; do
        git clone --depth 1 --branch ${BUSYBOX_VERSION} git://busybox.net/busybox.git busybox && { CLONE_SUCCESS=1; break; }
        echo "BusyBox clone failed, retrying ($i/3)..."
        sleep 10
    done
    if [ ${CLONE_SUCCESS} -eq 0 ]; then
        echo "ERROR: BusyBox clone failed after 3 attempts"
        exit 1
    fi
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Library dependencies"
${CROSS_COMPILE}readelf -a ./busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a ./busybox | grep "Shared library"

SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
cp -rL ${SYSROOT}/lib64/ ${OUTDIR}/rootfs/
cp -rL ${SYSROOT}/lib/ ${OUTDIR}/rootfs/

cd ${OUTDIR}/rootfs
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/tty c 5 1

cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

cd ${OUTDIR}/rootfs
cp ${FINDER_APP_DIR}/finder.sh home
cp ${FINDER_APP_DIR}/writer.sh home
cp ${FINDER_APP_DIR}/writer home
cp ${FINDER_APP_DIR}/finder-test.sh home
cp ${FINDER_APP_DIR}/autorun-qemu.sh home
cp -rL ${FINDER_APP_DIR}/conf home

cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio

cd ${OUTDIR}
gzip -f initramfs.cpio
