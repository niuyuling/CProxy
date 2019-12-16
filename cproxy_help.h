#ifndef CPROXY_HELP
#define CPROXY_HELP

#include <stdio.h>
#include "cproxy.h"
#define BUILD(fmt...)  do { fprintf(stderr,"%s %s ",__DATE__,__TIME__); fprintf(stderr, ##fmt); } while(0)

char help_information(void);

#endif
