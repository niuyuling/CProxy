CC = gcc
CFLAGS = -g -Wall -Werror -I../iniparser/src -L../iniparser
LIBS = -liniparser
OBJ := cproxy

all: cproxy.o cproxy_request.o conf.o
	$(CC) $(CFLAGS)  -o $(OBJ) $^ $(LIBS)
	strip $(OBJ)
	-chmod 755 $(OBJ)
.c.o:
	$(CC) $(CFLAGS) -c $< $(LIBS)

clean:
	rm -rf *.o
	rm $(OBJ)
