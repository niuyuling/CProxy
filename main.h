#ifndef MAIN_H
#define MAIN_H

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
#include <sched.h>
#include <getopt.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/resource.h>

#define MAX_CONNECTION 1020
#define BUFFER_SIZE 8192
#define PATH_SIZE 270
#define CACHE_SIZE 270

extern int local_port;
extern char local_host[128];
extern int process;

extern int epollfd;
extern struct epoll_event ev, events[MAX_CONNECTION + 1];
int create_connection(char *remote_host, int remote_port);
int create_connection6(char *remote_host, int remote_port);

#endif
