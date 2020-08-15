#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include "http_proxy.h"
#include "conf.h"

struct http_request {
    char *M, *U, *V;
    char *host, *port, *H;
    char *url, *uri;

    int M_len, U_len, V_len;
    int host_len, port_len, H_len;
    int url_len, uri_len;
};

void *memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen);
char *replace(char *replace_memory, int *replace_memory_len, const char *src, const int src_len, const char *dest, const int dest_len);
char *request_head(conn * in, conf * configure);

#endif
