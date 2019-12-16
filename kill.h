#ifndef KILL_H
#define KILL_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <pwd.h>
#include <regex.h>
#include <ctype.h>
#include <assert.h>
#include <locale.h>

#define COMM_LEN 64
#define OLD_COMM_LEN 16
#define _(String) (String)
#define rpmatch(line) \
	( (line == NULL)? -1 : \
	  (*line == 'y' || *line == 'Y')? 1 : \
	  (*line == 'n' || *line == 'N')? 0 : \
	  -1 )

#define PROC_BASE "/proc"
#define MAX_NAMES (int)(sizeof(unsigned long)*8)

#define TSECOND "s"
#define TMINUTE "m"
#define THOUR   "h"
#define TDAY    "d"
#define TWEEK   "w"
#define TMONTH  "M"
#define TYEAR   "y"

#define TMAX_SECOND 31536000
#define TMAX_MINUTE 525600
#define TMAX_HOUR   8760
#define TMAX_DAY    365
#define TMAX_WEEK   48
#define TMAX_MONTH  12
#define TMAX_YEAR   1

#define ER_REGFAIL -1
#define ER_NOMEM   -2
#define ER_UNKWN   -3
#define ER_OOFRA   -4

int kill_all(int signal, int name_count, char **namelist, struct passwd *pwent);

#endif
