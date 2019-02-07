CC = gcc
CFLAGS = -g -Wall -Werror -I../iniparser/src -L../iniparser
LIBS = -liniparser
OBJ := cproxy

all: cproxy.o conf.o cproxy_request.o cproxy_help.o
	$(CC) $(CFLAGS)  -o $(OBJ) $^ $(LIBS)
	strip $(OBJ)
	-chmod a+x $(OBJ)
.c.o:
	$(CC) $(CFLAGS) -c $< $(LIBS)

clean:
	rm -rf *.o
	rm $(OBJ)
