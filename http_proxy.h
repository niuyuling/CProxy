#ifndef HTTP_PROXY_H
#define HTTP_PROXY_H

#include "conf.h"
#include "main.h"

#define HTTP_TYPE 0
#define OTHER_TYPE 1

extern int remote_port;
extern char remote_host[128];
extern int sslEncodeCode;

typedef struct conn_t {
    int fd;
    char *header_buffer;
    int header_buffer_len, sent_len, timer;
    unsigned request_type :1;
} conn;

extern conn cts[MAX_CONNECTION];
extern void tcp_in(conn * in, conf * configure);
extern void tcp_out(conn * out);
extern void close_connection(conn * conn);

extern char *request_head(conn * in, conf * configure);
void dataEncode(char *data, int data_len);

#endif
