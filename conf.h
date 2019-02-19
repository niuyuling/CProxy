#ifndef CONF_H
#define CONF_H

#include "iniparser.h"
#include "cproxy.h"
char *strncpy_(char *dest, const char *src, size_t n);
void read_conf(char *file, conf *p);
void free_conf(conf *p);

#endif

