#include "conf.h"

void read_conf(char *file, conf *p)
{
    if(access(file, F_OK)) {
        printf("%s DOESN'T EXISIT!\n", file);
        exit(1);
    }
    
    dictionary *ini = iniparser_load(file);

    // server module
    // uid
    p->uid = iniparser_getint(ini, "server:uid", 0);
    //local_port
    p->server_port = iniparser_getint(ini, "server:local_port", 0);
    //pid_file
    p->len_server_pid_file = strlen(iniparser_getstring(ini, "server:pid_file", NULL)) + 1;
    p->server_pid_file = (char *)malloc(p->len_server_pid_file);
    if (p->server_pid_file == NULL) {
        goto err;
    }
    memset(p->server_pid_file, 0, p->len_server_pid_file);
    memcpy(p->server_pid_file, iniparser_getstring(ini, "server:pid_file", NULL), p->len_server_pid_file);
    //printf("%s\n", p->server_pid_file);

    // http module
    // http ip
    p->len_http_ip = strlen(iniparser_getstring(ini, "http:http_ip", NULL)) + 1;
    p->http_ip = (char *)malloc(p->len_http_ip);
    if (p->http_ip == NULL) {
        goto err;
    }
    memset(p->http_ip, 0, p->len_http_ip);
    memcpy(p->http_ip, iniparser_getstring(ini, "http:http_ip", NULL), p->len_http_ip);

    // http port
    p->http_port = iniparser_getint(ini, "http:http_port", 0);

    // http del
    p->len_http_del = strlen(iniparser_getstring(ini, "http:http_del", NULL)) + 1;
    p->http_del = (char *)malloc(p->len_http_del);
    if (p->http_del == NULL) {
        goto err;
    }
    memset(p->http_del, 0, p->len_http_del);
    memcpy(p->http_del, iniparser_getstring(ini, "http:http_del", NULL), p->len_http_del);

    // http first
    p->len_http_first = strlen(iniparser_getstring(ini, "http:http_first", NULL)) + 1;
    p->http_first = (char *)malloc(p->len_http_first);
    if (p->http_first == NULL) {
        goto err;
    }
    memset(p->http_first, 0, p->len_http_first);
    memcpy(p->http_first, iniparser_getstring(ini, "http:http_first", NULL), p->len_http_first);

    // https module
    // https ip
    p->len_https_ip = strlen(iniparser_getstring(ini, "https:https_ip", NULL)) + 1;
    p->https_ip = (char *)malloc(p->len_https_ip);
    if (p->https_ip == NULL) {
        goto err;
    }
    memset(p->https_ip, 0, p->len_http_ip);
    memcpy(p->https_ip, iniparser_getstring(ini, "https:https_ip", NULL), p->len_https_ip);

    //https port
    p->https_port = iniparser_getint(ini, "https:https_port", 0);
    
    // https del
    p->len_https_del = strlen(iniparser_getstring(ini, "https:https_del", NULL)) + 1;
    p->https_del = (char *)malloc(p->len_https_del);
    if (p->https_del == NULL) {
        goto err;
    }
    memset(p->https_del, 0, p->len_https_del);
    memcpy(p->https_del, iniparser_getstring(ini, "https:https_del", NULL), p->len_https_del);

    // https first
    p->len_https_first = strlen(iniparser_getstring(ini, "https:https_first", NULL)) + 1;
    p->https_first = (char *)malloc(p->len_https_first);
    if (p->https_first == NULL) {
        goto err;
    }
    memset(p->https_first, 0, p->len_https_first);
    memcpy(p->https_first, iniparser_getstring(ini, "https:https_first", NULL), p->len_https_first);

    
    // http strrep
    if (iniparser_find_entry(ini, "http:strrep") == 1) {
        p->len_http_strrep = strlen(iniparser_getstring(ini, "http:strrep", NULL)) + 1;
        p->http_strrep = (char *)malloc(p->len_http_strrep);
        if (p->http_strrep == NULL) {
            free(p->http_strrep);
        }
        memset(p->http_strrep, 0, p->len_http_strrep);
        memcpy(p->http_strrep, iniparser_getstring(ini, "http:strrep", NULL), p->len_http_strrep);
        char *p1 = strstr(p->http_strrep, "->");
        p->http_strrep_aim = (char *)malloc(strlen(p->http_strrep) - strlen(p1+2) -2 + 1);
        if (p->http_strrep_aim == NULL) {
            free(p->http_strrep_aim);   
        }
        strncpy_(p->http_strrep_aim, p->http_strrep, strlen(p->http_strrep) - strlen(p1+2) - 2);
        p->http_strrep_obj = (char *)malloc(strlen(p1+2) + 1);
        if (p->http_strrep_obj == NULL) {
            free(p->http_strrep_obj);
        }
        strncpy_(p->http_strrep_obj, p1+2, strlen(p1+2));
    }
    
    // https strrep
    if (iniparser_find_entry(ini, "https:strrep") == 1) {
        p->len_https_strrep = strlen(iniparser_getstring(ini, "https:strrep", NULL)) + 1;
        p->https_strrep = (char *)malloc(p->len_https_strrep);
        if (p->https_strrep == NULL) {
            free(p->https_strrep);
        }
        memset(p->https_strrep, 0, p->len_https_strrep);
        memcpy(p->https_strrep, iniparser_getstring(ini, "https:strrep", NULL), p->len_https_strrep);
        char *p2 = strstr(p->https_strrep, "->");
        p->https_strrep_aim = (char *)malloc(strlen(p->https_strrep) - strlen(p2+2) -2 + 1);
        if (p->https_strrep_aim == NULL) {
            free(p->https_strrep_aim);
        }
        strncpy_(p->https_strrep_aim, p->https_strrep, strlen(p->https_strrep) - strlen(p2+2) - 2);
        p->https_strrep_obj = (char *)malloc(strlen(p2+2) + 1);
        if (p->https_strrep_obj == NULL) {
            free(p->https_strrep_obj);
        }
        strncpy_(p->https_strrep_obj, p2+2, strlen(p2+2));
    }
    
    // http regrep
    if (iniparser_find_entry(ini, "http:regrep") == 1) {
        p->len_http_regrep = strlen(iniparser_getstring(ini, "http:regrep", NULL)) + 1;
        p->http_regrep = (char *)malloc(p->len_http_regrep);
        if (p->http_regrep == NULL) {
            free(p->http_regrep);
        }
        memset(p->http_regrep, 0, p->len_http_regrep);
        memcpy(p->http_regrep, iniparser_getstring(ini, "http:regrep", NULL), p->len_http_regrep);
        char *p3 = strstr(p->http_regrep, "->");
        p->http_regrep_aim = (char *)malloc(strlen(p->http_regrep) - strlen(p3+2) -2 + 1);
        if (p->http_regrep_aim == NULL) {
            free(p->http_regrep_aim);
        }
        strncpy_(p->http_regrep_aim, p->http_regrep, strlen(p->http_regrep) - strlen(p3+2) - 2);
        p->http_regrep_obj = (char *)malloc(strlen(p3+2) + 1);
        if (p->http_regrep_obj == NULL) {
            free(p->http_regrep_obj);
        }
        strncpy_(p->http_regrep_obj, p3+2, strlen(p3+2));
    }
    
    // https regrep
    if (iniparser_find_entry(ini, "https:regrep") == 1) {
        p->len_https_regrep = strlen(iniparser_getstring(ini, "https:regrep", NULL)) + 1;
        p->https_regrep = (char *)malloc(p->len_https_regrep);
        if (p->https_regrep == NULL) {
            free(p->https_regrep);
        }
        memset(p->https_regrep, 0, p->len_https_regrep);
        memcpy(p->https_regrep, iniparser_getstring(ini, "https:regrep", NULL), p->len_https_regrep);
        char *p4 = strstr(p->https_regrep, "->");
        p->https_regrep_aim = (char *)malloc(strlen(p->https_regrep) - strlen(p4+2) -2 + 1);
        if (p->https_regrep_aim == NULL) {
            free(p->https_regrep_aim);
        }
        strncpy_(p->https_regrep_aim, p->https_regrep, strlen(p->https_regrep) - strlen(p4+2) - 2);
        p->https_regrep_obj = (char *)malloc(strlen(p4+2) + 1);
        if (p->https_regrep_obj == NULL) {
            free(p->https_regrep_obj);
        }
        strncpy_(p->https_regrep_obj, p4+2, strlen(p4+2));
    }
    
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

void free_conf(conf *p)
{
    free(p->server_pid_file);
    free(p->http_ip);
    free(p->http_del);
    free(p->http_first);
    free(p->https_ip);
    free(p->https_del);
    free(p->https_first);
    
    free(p->http_strrep);
    free(p->http_strrep_aim);
    free(p->http_strrep_obj);
    free(p->https_strrep);
    free(p->https_strrep_aim);
    free(p->https_strrep_obj);
    
    free(p->http_regrep);
    free(p->http_regrep_aim);
    free(p->http_regrep_obj);
    free(p->https_regrep);
    free(p->https_regrep_aim);
    free(p->https_regrep_obj);
}

