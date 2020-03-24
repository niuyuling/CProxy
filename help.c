#include "help.h"

char help_information(void)
{
    static const char name[] = "CProxy";
    static const char subject[] = "proxy server";
    static const struct {
        const char *email;
    } author = {
        "aixiao@aixiao.me",
    };

    static const char usage[] = "Usage: [-?hpt] [-s signal] [-c filename]";
    static const char *s_help[] = {
        "",
        "Options:",
        "    -l --local_address     : localip:localport",
        "    -f --remote_address    : remoteip:remote:port",
        "    -p --process           : process number, default: 2",
        "    -t --timeout           : timeout minute, default: no timeout",
        "    -e --coding            : ssl coding, default: [0-128]",
        "    -s --signal            : send signal to a master process: stop, quit, restart, reload, status",
        "    -c --config            : set configuration file, default: CProxy.ini",
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
