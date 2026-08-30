CC=$(CROSS_COMPILE)gcc

edit: writer.o

all: writer

writer: writer.o
    $(CC) writer.o -o writer

writer.o: writer.c
	$(CC) writer.c -o writer

clean:
	rm -rf *.o test.txt writer