CROSS_COMPILE ?= 
CC := $(CROSS_COMPILE)gcc
STRIP := $(CROSS_COMPILE)strip
CFLAGS := -O2 -pthread -Wall
BIN := udpServer

all : udpServer.o
	$(CC) $(CFLAGS) -o $(BIN) $^
	$(STRIP) $(BIN)
	-chmod a+x $(BIN)

clean : 
	rm -f *.o $(BIN)
