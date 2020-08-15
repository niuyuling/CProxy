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
    int dns_listen;

    // http module
    int http_port;
    char *http_ip, *http_del, *http_first;
    int http_ip_len, http_del_len, http_first_len;
    char *http_strrep, *http_regrep;
    int http_strrep_len, http_regrep_len;

    char *http_strrep_aim, *http_strrep_obj;
    int http_strrep_aim_len, http_strrep_obj_len;

    char *http_regrep_aim, *http_regrep_obj;
    int http_regrep_aim_len, http_regrep_obj_len;

    // https module
    int https_port;
    char *https_ip, *https_del, *https_first;
    int https_ip_len, https_del_len, https_first_len;
    char *https_strrep, *https_regrep;
    int https_strrep_len, https_regrep_len;

    char *https_strrep_aim, *https_strrep_obj;
    int https_strrep_aim_len, https_strrep_obj_len;

    char *https_regrep_aim, *https_regrep_obj;
    int https_regrep_aim_len, https_regrep_obj_len;
    
        
    // http dns_listen
    char *addr;
    char *http_req;
    int http_req_len;
    int encode;
} conf;

char *strncpy_(char *dest, const char *src, size_t n);
void read_conf(char *file, conf * p);
void free_conf(conf * p);


#endif
