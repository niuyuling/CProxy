#include "conf.h"

void read_conf(char *file, conf * p)
{
    dictionary *ini = iniparser_load(file);

    // server module
    p->server_port = iniparser_getint(ini, "server:PORT", 0);
    p->len_server_pid_file =
        strlen(iniparser_getstring(ini, "server:PID_FILE", NULL)) + 1;
    p->server_pid_file = (char *)malloc(p->len_server_pid_file);
    if (p->server_pid_file == NULL) {
        goto err;
    }
    memset(p->server_pid_file, 0, p->len_server_pid_file);
    memcpy(p->server_pid_file,
           iniparser_getstring(ini, "server:PID_FILE", NULL),
           p->len_server_pid_file);

    // http module
    p->len_http_ip = strlen(iniparser_getstring(ini, "http:http_ip", NULL)) + 1;
    p->http_ip = (char *)malloc(p->len_http_ip);
    if (p->http_ip == NULL) {
        goto err;
    }
    memset(p->http_ip, 0, p->len_http_ip);
    memcpy(p->http_ip, iniparser_getstring(ini, "http:http_ip", NULL),
           p->len_http_ip);

    p->http_port = iniparser_getint(ini, "http:http_port", 0);

    p->len_http_del =
        strlen(iniparser_getstring(ini, "http:http_del", NULL)) + 1;
    p->http_del = (char *)malloc(p->len_http_del);
    if (p->http_del == NULL) {
        goto err;
    }
    memset(p->http_del, 0, p->len_http_del);
    memcpy(p->http_del, iniparser_getstring(ini, "http:http_del", NULL),
           p->len_http_del);

    p->len_http_first =
        strlen(iniparser_getstring(ini, "http:http_first", NULL)) + 1;
    p->http_first = (char *)malloc(p->len_http_first);
    if (p->http_first == NULL) {
        goto err;
    }
    memset(p->http_first, 0, p->len_http_first);
    memcpy(p->http_first, iniparser_getstring(ini, "http:http_first", NULL),
           p->len_http_first);

    // https module
    p->len_https_ip =
        strlen(iniparser_getstring(ini, "https:https_ip", NULL)) + 1;
    p->https_ip = (char *)malloc(p->len_https_ip);
    if (p->https_ip == NULL) {
        goto err;
    }
    memset(p->https_ip, 0, p->len_http_ip);
    memcpy(p->https_ip, iniparser_getstring(ini, "https:https_ip", NULL),
           p->len_https_ip);

    p->https_port = iniparser_getint(ini, "https:https_port", 0);

    p->len_https_del =
        strlen(iniparser_getstring(ini, "https:https_del", NULL)) + 1;
    p->https_del = (char *)malloc(p->len_https_del);
    if (p->https_del == NULL) {
        goto err;
    }
    memset(p->https_del, 0, p->len_https_del);
    memcpy(p->https_del, iniparser_getstring(ini, "https:https_del", NULL),
           p->len_https_del);

    p->len_https_first =
        strlen(iniparser_getstring(ini, "https:https_first", NULL)) + 1;
    p->https_first = (char *)malloc(p->len_https_first);
    if (p->https_first == NULL) {
        goto err;
    }
    memset(p->https_first, 0, p->len_https_first);
    memcpy(p->https_first, iniparser_getstring(ini, "https:https_first", NULL),
           p->len_https_first);

err:
    if (p->server_pid_file == NULL)
        free(p->server_pid_file);
    if (p->http_ip == NULL)
        free(p->http_ip);
    if (p->http_del == NULL)
        free(p->http_del);
    if (p->http_first == NULL)
        free(p->http_first);
    if (p->https_ip == NULL)
        free(p->https_ip);
    if (p->https_del == NULL)
        free(p->https_del);
    if (p->https_first == NULL)
        free(p->https_first);

    iniparser_freedict(ini);
}

void free_conf(conf * p)
{
    free(p->server_pid_file);
    free(p->http_ip);
    free(p->http_del);
    free(p->http_first);
    free(p->https_ip);
    free(p->https_del);
    free(p->https_first);
}
