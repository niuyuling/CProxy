#ifndef HTTPDNS_H
#define HTTPDNS_H

#include "main.h"

#define HTTPDNS_REQUEST "GET /d?dn=[D] HTTP/1.0\r\nHost: [H]\r\n\r\n"

struct httpdns {
    struct sockaddr_in dst;
    char *http_req, *original_http_req, *connect_request, *original_connect_request, *cachePath, *ssl_request; //original_http_req, original_connect_request为初始化生成的请求头，用来配合use_hdr语法
    int http_req_len, original_http_req_len, connect_request_len, original_connect_request_len, cacheLimit, ssl_request_len;
    unsigned encodeCode,        //Host编码传输
     httpsProxy_encodeCode,     //CONNECT代理编码
     tcpDNS_mode:1;             //判断是否开启TCPDNS
};

extern void dns_timeout_check();
extern void *dns_loop(void *nullPtr);
extern int8_t read_cache_file();
extern void dns_init();
extern struct httpdns httpdns;
extern pid_t child_pid;
extern FILE *cfp;

#endif
