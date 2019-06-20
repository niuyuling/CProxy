#ifndef CPROXY_REQUEST_H
#define CPROXY_REQUEST_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include "cproxy.h"

void *memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen);
char *replace(char *replace_memory, int *replace_memory_len, const char *src, const int src_len, const char *dest, const int dest_len);
void del_chr(char *s, char ch);
char *strncpy_(char *dest, const char *src, size_t n);
uint8_t request_type(char *req);
int extract_host(char *header);
void forward_header(int destination_sock);
void rewrite_header();
int numbin(int n);
char *splice_head(char *header_buffer, const char *character, char *string);
char *delete_header(char *header_buffer, const char *character, int string);
int replacement_http_head(char *header_buffer, char *remote_host, int *remote_port, int *SIGN, conf *p);

#endif

