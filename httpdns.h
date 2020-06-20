#ifndef _HTTPDNS_
#define _HTTPDNS_

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <string.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#define DEFAULT_UPPER_IP "114.114.114.114"
#define MAX_FD 1024
#define DNS_REQUEST_SIZE 512
#define BUFF_SIZE 1024

typedef struct dns_request {
    char dns_req[DNS_REQUEST_SIZE + 1];
    char *http_rsp;
    unsigned int http_rsp_len, sent_len, dns_req_len;
    int fd;
} dns_t;

struct dns_hosts {
    char *host;
    char *ip;
    struct dns_hosts *next;
};

#define VERSION "0.3"
#define ERROR_MSG "HTTP/1.0 404 Not Found\r\nConnection: close\r\nVia: Mmmdbybyd(HTTP-DNS Server "VERSION")\r\nContent-type: charset=utf-8\r\n\r\n<html><head><title>HTTP DNS Server</title></head><body>查询域名失败<br/><br/>By: 萌萌萌得不要不要哒<br/>E-mail: 915445800@qq.com</body></html>"
#define SUCCESS_HEADER "HTTP/1.0 200 OK\r\nConnection: close\r\nVia: Mmmdbybyd(HTTP-DNS Server "VERSION")\r\n\r\n"

dns_t dns_list[MAX_FD - 2];     //监听客户端FD DNS服务端fd
struct epoll_event evs[MAX_FD - 1], ev;

int listenFd, dstFd, eFd;
socklen_t addr_len;
void recv_dns_rsp();
void query_dns();
void read_client(dns_t * in);
void response_client(dns_t * out);
void httpdns_accept_client();
int httpdns_initialize();
void *httpdns_start_server();

#endif
