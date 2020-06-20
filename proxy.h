#ifndef PROXY_H
#define PROXY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <pwd.h>
#include <dirent.h>
#include <pthread.h>
#include <getopt.h>
#include <netinet/in.h>

#define MAX_CONNECTION 1020
#define BUFFER_SIZE 10240
#define PATH_SIZE 270

int local_port;
char local_host[128];
int process;

extern int epollfd;
extern struct epoll_event ev, events[MAX_CONNECTION + 1];
int create_connection(char *remote_host, int remote_port);

#endif
