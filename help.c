#include "help.h"

char help_information(void)
{
    static const char name[] = "C";
    static const char subject[] = "Proxy Server";
    static const struct {
        const char *email;
    } author = {
        "AIXIAO@AIXIAO.ME",
    };

    static const char usage[] = "Usage: [-?h] [-s signal] [-c filename]";
    static const char *s_help[] = {
        "",
        "Options:",
        "    -s --signal            : send signal to a master process: stop, quit, restart, reload, status",
        "    -c --config            : set configuration file, default: CProxy.conf",
        "    -? -h --? --help       : help information",
        "",
        0
    };

    fprintf(stderr, "%s %s\n", name, subject);
    fprintf(stderr, "Author: %s\n", author.email);
    fprintf(stderr, "%s\n", usage);

    int l;
    for (l = 0; s_help[l]; l++) {
        fprintf(stderr, "%s\n", s_help[l]);
    }

    BUILD("Compile、link.\n");

    return 0;
}
