CC=g++
CFLAGS=-c -Wall

all: main

main: main.o
	$(CC) main.o -o main

main.o: main.cc
	$(CC) $(CFLAGS) main.cc

clean:
	rm -rf *o main
