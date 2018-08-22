CC = gcc
CFLAGS = -g -Wall -Werror

all: cproxy

cproxy: cproxy.o
	$(CC) $(CFLAGS) cproxy.o -o cproxy

cproxy.o:
	$(CC) $(CFLAGS) -c cproxy.c

clean:
	rm -rf *.o
	rm cproxy