#!/bin/sh
# Authour: shri

set -e
set -u

if [ $# -ne 2 ]; then
	echo usage: writer.sh "<file>" "<string>"
	exit 1
fi

writefile=$1
writestr=$2

mkdir -p "$(dirname "$writefile")" || {
	echo "Error Cant create file"
	exit 1
}

echo "$writestr" > "$writefile" || {
	echo "Error writer into file"
        exit 1
}


