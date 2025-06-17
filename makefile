CC = gcc
CFLAGS = -Wall

all: hw2

hw1: hw2.c
	$(CC) $(CFLAGS) hw2.c -o hw2

clean:
	rm -f hw2
