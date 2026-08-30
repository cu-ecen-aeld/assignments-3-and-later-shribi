CC=$(CROSS_COMPILE)gcc

CFLAGS=-Wall -Wextra -Werror -O2 -g

default: all

all: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) -o aesdsocket aesdsocket.c

clean:
	rm -rf *.o aesdsocket