CC ?= $(CROSS_COMPILE)gcc

CFLAGS ?= -Wall -Wextra -Werror -O2 -g -pthread

LDFLAGS ?= -lpthread

TARGET ?= aesdsocket

USE_AESD_CHAR_DEVICE ?= 1

default: all

all: $(TARGET)

$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) -DUSE_AESD_CHAR_DEVICE=$(USE_AESD_CHAR_DEVICE) -o $(TARGET) aesdsocket.c $(LDFLAGS)

clean:
	rm -rf *.o $(TARGET)