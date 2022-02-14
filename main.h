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
#define CACHE_SIZE 270
#define ERRDEBUG fprintf(stderr,"Error Occured at File: %s, Function: %s, Line: %d, Date: %s, Time: %s.\n", __FILE__, __FUNCTION__, __LINE__, __DATE__, __TIME__);


struct global {
    int tcp_listen_fd, dns_listen_fd, udp_listen_fd, uid, procs, timeout_m;
    unsigned mode:3, strict_modify:1;
};

struct save_header {
    struct save_header *next;
    char *key, *value, *replace_string;
    int key_len, value_len, replace_string_len, updateTime, timer;
    unsigned notUpdate:1;
};

struct modify {
    char *first, *del_hdr, *src, *dest;
    struct save_header *saveHdr;
    struct modify *next;
    int first_len, del_hdr_len, src_len, dest_len;
    unsigned flag:3;            //判断修改请求头的操作
};

struct tcp_mode {
    struct sockaddr_in dst;
    struct modify *m;
    unsigned encodeCode,        //wap_connect模式数据编码传输
     uri_strict:1, http_only_get_post:1;
};

extern struct global global;
extern struct tcp_mode https;
extern struct save_header *saveHdrs;
extern int default_ssl_request_len;
extern char *default_ssl_request;

extern char local_host[CACHE_SIZE];
extern int epollfd, local_port, process;
extern struct epoll_event ev, events[MAX_CONNECTION + 1];
int create_connection(char *remote_host, int remote_port);
int create_connection6(char *remote_host, int remote_port);
extern uint16_t tcp_listen_port;

#endif
