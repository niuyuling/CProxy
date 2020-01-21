CROSS_COMPILE ?= 
CC := $(CROSS_COMPILE)gcc
STRIP := $(CROSS_COMPILE)strip
CFLAGS += -g -O2 -Wall -I../iniparser/src -L../iniparser
LIBS = -liniparser -pthread -static
OBJ := CProxy

all: proxy.o http.o request.o picohttpparser.o conf.o timeout.o help.o
	$(CC) $(CFLAGS) -o $(OBJ) $^ $(LIBS)
.c.o:
	$(CC) $(CFLAGS) -c $< $(LIBS)

clean:
	rm -rf *.o
	rm $(OBJ)

