#ifndef HELP_H
#define HELP_H

#include <stdio.h>
#define BUILD(fmt...)  do { fprintf(stderr,"%s %s ",__DATE__,__TIME__); fprintf(stderr, ##fmt); } while(0)

char help_information(void);

#endif
