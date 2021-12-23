#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include "http_proxy.h"
#include "conf.h"

struct http_request {
    char *method, *U, *version;
    char *host, *port, *H;
    char *url, *uri;

    int method_len, U_len, version_len;
    int host_len, port_len, H_len;
    int url_len, uri_len;
};

void *memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen);
extern char *replace(char *replace_memory, int *replace_memory_len, const char *src, const int src_len, const char *dest, const int dest_len);
char *request_head(conn_t * in, conf * configure);
extern int8_t copy_new_mem(char *src, int src_len, char **dest);
extern void errors(const char *msg);

#endif
