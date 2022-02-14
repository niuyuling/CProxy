#ifndef CONF_H
#define CONF_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <error.h>
#include <unistd.h>

// 配置文件结构
typedef struct CONF {
    // server module
    int uid;
    int process;
    int timeout;
    int sslencoding;
    //int server_port;
    int tcp_listen;
    int tcp6_listen;
    int dns_listen;
    int udp_listen;

    // http module
    int http_port;
    char *http_ip, *http_del, *http_first;
    int http_ip_len, http_del_len, http_first_len;

    // https module
    int https_port;
    char *https_ip, *https_del, *https_first;
    int https_ip_len, https_del_len, https_first_len;

    // httpdns module
    char *addr;
    char *http_req;
    int addr_len;
    int http_req_len;
    int encode;
    
    // httpudp module
    char *httpudp_addr;
    char *httpudp_http_req;
    int httpudp_addr_len;
    int httpudp_http_req_len;
    int httpudp_encode;
} conf;

typedef struct tcp {
    char *strrep;
    int strrep_len;
    char *strrep_s, *strrep_t;
    int strrep_s_len, strrep_t_len;

    char *regrep;
    int regrep_len;
    char *regrep_s, *regrep_t;
    int regrep_s_len, regrep_t_len;

    struct tcp *next;
} tcp;

extern tcp *http_head_strrep;
extern tcp *http_head_regrep;
extern tcp *http_node;

extern tcp *https_head_strrep;
extern tcp *https_head_regrep;
extern tcp *https_node;

extern void print_tcp(tcp *p);
extern void free_tcp(tcp **p);
extern tcp *local_reverse(tcp *head);


char *strncpy_(char *dest, const char *src, size_t n);
void read_conf(char *file, conf *p);
void free_conf(conf *p);

#endif
