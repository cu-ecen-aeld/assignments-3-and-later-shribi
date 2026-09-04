CC ?= $(CROSS_COMPILE)gcc

CFLAGS ?= -Wall -Wextra -Werror -O2 -g -pthread

LDFLAGS ?= -lpthread

TARGET ?= aesdsocket

default: all

all: $(TARGET)

$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) -o $(TARGET) aesdsocket.c $(LDFLAGS)

clean:
	rm -rf *.o $(TARGET)