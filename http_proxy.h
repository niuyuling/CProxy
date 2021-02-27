#ifndef HTTP_PROXY_H
#define HTTP_PROXY_H

#include "conf.h"
#include "main.h"
#include <arpa/inet.h>

#define HTTP_TYPE 0
#define OTHER_TYPE 1

extern int remote_port;
extern char remote_host[CACHE_SIZE];
extern int sslEncodeCode;

typedef struct tcp_connection {
    char *ready_data, *incomplete_data;
    int fd, ready_data_len, incomplete_data_len, sent_len, timer;
    uint16_t destPort;
    unsigned reread_data:1, request_type:1, keep_alive:1;
} conn_t;

extern conn_t cts[MAX_CONNECTION];
extern void tcp_in(conn_t * in, conf * configure);
extern void tcp_out(conn_t * out);
extern void close_connection(conn_t * conn);

extern char *request_head(conn_t * in, conf * configure);
void dataEncode(char *data, int data_len);

#endif
