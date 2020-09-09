#ifndef _HTTPDNS_
#define _HTTPDNS_

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>

#include "conf.h"

#define DNS_MAX_CONNECTION 256 //此值的大小关系到respod_clients函数的效率
#define DATA_SIZE 512
#define HTTP_RSP_SIZE 1024


typedef struct dns_connection {
    char dns_req[DATA_SIZE];
    struct sockaddr_in src_addr;
    char *reply; //回应内容
    char *http_request, *host;
    unsigned int http_request_len, dns_rsp_len;
    int fd;
    char query_type;
    unsigned host_len :7; //域名最大长度64位
    unsigned wait_response_client :1; //已构建好DNS回应，等待可写事件
} dns_t;

struct dns_cache {
    int question_len;
    char *question;
    char *answer;
    struct dns_cache *next;
};

extern dns_t dns_list[DNS_MAX_CONNECTION];
extern struct epoll_event evs[DNS_MAX_CONNECTION+1], event;


void *httpdns_loop(void *p);
int httpdns_initialize(conf * configure);


#endif
