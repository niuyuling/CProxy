CROSS_COMPILE ?= 
CC := $(CROSS_COMPILE)gcc
STRIP := $(CROSS_COMPILE)strip
CFLAGS += -g -O2 -Wall -pthread
LIBS = 
OBJ := CProxy

all: main.o http_proxy.o httpdns.o http_request.o conf.o timeout.o kill.o help.o
	$(CC) $(CFLAGS) -o $(OBJ) $^ $(LIBS)
.c.o:
	$(CC) $(CFLAGS) -c $< $(LIBS)

clean:
	rm -rf *.o
	rm $(OBJ)

android:
	ndk-build NDK_PROJECT_PATH=. NDK_APPLICATION_MK=Application.mk APP_BUILD_SCRIPT=Android.mk
