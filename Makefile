CROSS_COMPILE ?= 
CC := $(CROSS_COMPILE)gcc
STRIP := $(CROSS_COMPILE)strip
CFLAGS += -g -O2 -Wall -I../iniparser/src -L../iniparser
LIBS = -liniparser -static
OBJ := cproxy

all: cproxy.o conf.o cproxy_request.o cproxy_help.o kill.o
	$(CC) $(CFLAGS) -o $(OBJ) $^ $(LIBS)
	$(STRIP) $(OBJ)
	-chmod a+x $(OBJ)
.c.o:
	$(CC) $(CFLAGS) -c $< $(LIBS)

clean:
	rm -rf *.o
	rm $(OBJ)
