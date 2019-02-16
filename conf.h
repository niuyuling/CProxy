#ifndef CONF_H
#define CONF_H

#include "iniparser.h"
#include "cproxy.h"
void read_conf(char *file, conf *p);
void free_conf(conf *p);

#endif

