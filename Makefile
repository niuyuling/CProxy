CC = gcc
CFLAGS = -g -Wall -Werror -I./iniparser/src -L ./iniparser
LIBS = -liniparser

all: cproxy

cproxy: cproxy.o
	$(CC) $(CFLAGS) cproxy.o cproxy_request.o -o cproxy $(LIBS)

cproxy.o:
	$(CC) $(CFLAGS) -c cproxy.c cproxy_request.c

clean:
	rm -rf *.o
	rm cproxy