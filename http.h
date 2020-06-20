#ifndef HTTP_H
#define HTTP_H

#include "conf.h"
#include "proxy.h"

#define HTTP_TYPE 0
#define OTHER_TYPE 1

int remote_port;
char remote_host[128];


extern int sslEncodeCode;

typedef struct conn_t {
    int fd;
    char *header_buffer;
    int header_buffer_len, sent_len, timer;
} conn;

extern conn cts[MAX_CONNECTION];
extern void tcp_in(conn * in, conf * configure);
extern void tcp_out(conn * out);
extern void clienttoserver(conn * in);
extern void close_connection(conn * conn);

extern char *request_head(conn * in, conf * configure);
void dataEncode(char *data, int data_len);

#endif
