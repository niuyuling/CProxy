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
#include <errno.h>
#include <locale.h>

#ifndef I18N_H
#define I18N_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef ENABLE_NLS
#include <locale.h>
#include <libintl.h>
#define _(String) gettext (String)
#else
#define _(String) (String)
#endif

#endif

#ifndef HAVE_RPMATCH
#define rpmatch(line) \
	( (line == NULL)? -1 : \
	  (*line == 'y' || *line == 'Y')? 1 : \
	  (*line == 'n' || *line == 'N')? 0 : \
	  -1 )
#endif

#define COMM_LEN 64
#define PROC_BASE "/proc"
#define NUM_NS 6

int kill_all (int signal, int name_count, char **namelist, struct passwd *pwent);

#endif
