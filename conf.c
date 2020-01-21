#include "conf.h"

char *strncpy_(char *dest, const char *src, size_t n)
{
    int size = sizeof(char) * (n + 1);
    char *tmp = (char *)malloc(size); // 开辟大小为n+1的临时内存tmp
    if (tmp) {
        memset(tmp, '\0', size); // 将内存初始化为0
        memcpy(tmp, src, size - 1); // 将src的前n个字节拷贝到tmp
        memcpy(dest, tmp, size); // 将临时空间tmp的内容拷贝到dest
        free(tmp);              // 释放内存
        return dest;
    } else {
        return NULL;
    }
}

void read_conf(char *file, conf * p)
{
    if (access(file, F_OK)) {
        printf("%s DOESN'T EXISIT!\n", file);
        exit(1);
    }

    dictionary *ini = iniparser_load(file);

    // server module
    // uid
    p->uid = iniparser_getint(ini, "server:uid", 0);
    
    // process
    p->process = iniparser_getint(ini, "server:process", 0);
    
    // timer
    p->timer = iniparser_getint(ini, "server:timer", 0);
    
    //local_port
    p->server_port = iniparser_getint(ini, "server:local_port", 0);
    //pid_file
    p->server_pid_file_len = strlen(iniparser_getstring(ini, "server:pid_file", NULL)) + 1;
    p->server_pid_file = (char *)malloc(p->server_pid_file_len);
    if (p->server_pid_file == NULL) {
        goto err;
    }
    memset(p->server_pid_file, 0, p->server_pid_file_len);
    memcpy(p->server_pid_file, iniparser_getstring(ini, "server:pid_file", NULL), p->server_pid_file_len);



    // http module
    // http ip
    p->http_ip_len = strlen(iniparser_getstring(ini, "http:http_ip", NULL)) + 1;
    p->http_ip = (char *)malloc(p->http_ip_len);
    if (p->http_ip == NULL) {
        goto err;
    }
    memset(p->http_ip, 0, p->http_ip_len);
    memcpy(p->http_ip, iniparser_getstring(ini, "http:http_ip", NULL), p->http_ip_len);

    // http port
    p->http_port = iniparser_getint(ini, "http:http_port", 0);

    // http del
    p->http_del_len = strlen(iniparser_getstring(ini, "http:http_del", NULL)) + 1;
    p->http_del = (char *)malloc(p->http_del_len);
    if (p->http_del == NULL) {
        goto err;
    }
    memset(p->http_del, 0, p->http_del_len);
    memcpy(p->http_del, iniparser_getstring(ini, "http:http_del", NULL), p->http_del_len);

    // http first
    p->http_first_len = strlen(iniparser_getstring(ini, "http:http_first", NULL)) + 1;
    p->http_first = (char *)malloc(p->http_first_len);
    if (p->http_first == NULL) {
        goto err;
    }
    memset(p->http_first, 0, p->http_first_len);
    memcpy(p->http_first, iniparser_getstring(ini, "http:http_first", NULL), p->http_first_len);

    // http strrep
    if (iniparser_find_entry(ini, "http:strrep") == 1) {
        p->http_strrep_len = strlen(iniparser_getstring(ini, "http:strrep", NULL)) + 1;
        p->http_strrep = (char *)malloc(p->http_strrep_len);
        if (p->http_strrep == NULL) {
            free(p->http_strrep);
        }
        memset(p->http_strrep, 0, p->http_strrep_len);
        memcpy(p->http_strrep, iniparser_getstring(ini, "http:strrep", NULL), p->http_strrep_len);
        char *p1 = strstr(p->http_strrep, "->");
        p->http_strrep_aim = (char *)malloc(strlen(p->http_strrep) - strlen(p1 + 2) - 2 + 1);
        if (p->http_strrep_aim == NULL) {
            free(p->http_strrep_aim);
        }
        strncpy_(p->http_strrep_aim, p->http_strrep, strlen(p->http_strrep) - strlen(p1 + 2) - 2);
        p->http_strrep_obj = (char *)malloc(strlen(p1 + 2) + 1);
        if (p->http_strrep_obj == NULL) {
            free(p->http_strrep_obj);
        }
        strncpy_(p->http_strrep_obj, p1 + 2, strlen(p1 + 2));
        p->http_strrep_aim_len = strlen(p->http_strrep_aim);
        p->http_strrep_obj_len = strlen(p->http_strrep_obj);
    }
    
    // http regrep
    if (iniparser_find_entry(ini, "http:regrep") == 1) {
        p->http_regrep_len = strlen(iniparser_getstring(ini, "http:regrep", NULL)) + 1;
        p->http_regrep = (char *)malloc(p->http_regrep_len);
        if (p->http_regrep == NULL) {
            free(p->http_regrep);
        }
        memset(p->http_regrep, 0, p->http_regrep_len);
        memcpy(p->http_regrep, iniparser_getstring(ini, "http:regrep", NULL), p->http_regrep_len);
        char *p3 = strstr(p->http_regrep, "->");
        p->http_regrep_aim =
            (char *)malloc(strlen(p->http_regrep) - strlen(p3 + 2) - 2 + 1);
        if (p->http_regrep_aim == NULL) {
            free(p->http_regrep_aim);
        }
        strncpy_(p->http_regrep_aim, p->http_regrep,
                 strlen(p->http_regrep) - strlen(p3 + 2) - 2);
        p->http_regrep_obj = (char *)malloc(strlen(p3 + 2) + 1);
        if (p->http_regrep_obj == NULL) {
            free(p->http_regrep_obj);
        }
        strncpy_(p->http_regrep_obj, p3 + 2, strlen(p3 + 2));
        p->http_regrep_aim_len = strlen(p->http_regrep_aim);
        p->http_regrep_obj_len = strlen(p->http_regrep_obj);
    }



    // https module
    // https ip
    p->https_ip_len = strlen(iniparser_getstring(ini, "https:https_ip", NULL)) + 1;
    p->https_ip = (char *)malloc(p->https_ip_len);
    if (p->https_ip == NULL) {
        goto err;
    }
    memset(p->https_ip, 0, p->https_ip_len);
    memcpy(p->https_ip, iniparser_getstring(ini, "https:https_ip", NULL), p->https_ip_len);

    //https port
    p->https_port = iniparser_getint(ini, "https:https_port", 0);

    // https del
    p->https_del_len = strlen(iniparser_getstring(ini, "https:https_del", NULL)) + 1;
    p->https_del = (char *)malloc(p->https_del_len);
    if (p->https_del == NULL) {
        goto err;
    }
    memset(p->https_del, 0, p->https_del_len);
    memcpy(p->https_del, iniparser_getstring(ini, "https:https_del", NULL), p->https_del_len);

    // https first
    p->https_first_len = strlen(iniparser_getstring(ini, "https:https_first", NULL)) + 1;
    p->https_first = (char *)malloc(p->https_first_len);
    if (p->https_first == NULL) {
        goto err;
    }
    memset(p->https_first, 0, p->https_first_len);
    memcpy(p->https_first, iniparser_getstring(ini, "https:https_first", NULL), p->https_first_len);

    // https strrep
    if (iniparser_find_entry(ini, "https:strrep") == 1) {
        p->https_strrep_len = strlen(iniparser_getstring(ini, "https:strrep", NULL)) + 1;
        p->https_strrep = (char *)malloc(p->https_strrep_len);
        if (p->https_strrep == NULL) {
            free(p->https_strrep);
        }
        memset(p->https_strrep, 0, p->https_strrep_len);
        memcpy(p->https_strrep, iniparser_getstring(ini, "https:strrep", NULL), p->https_strrep_len);
        char *p2 = strstr(p->https_strrep, "->");
        p->https_strrep_aim = (char *)malloc(strlen(p->https_strrep) - strlen(p2 + 2) - 2 + 1);
        if (p->https_strrep_aim == NULL) {
            free(p->https_strrep_aim);
        }
        strncpy_(p->https_strrep_aim, p->https_strrep, strlen(p->https_strrep) - strlen(p2 + 2) - 2);
        p->https_strrep_obj = (char *)malloc(strlen(p2 + 2) + 1);
        if (p->https_strrep_obj == NULL) {
            free(p->https_strrep_obj);
        }
        strncpy_(p->https_strrep_obj, p2 + 2, strlen(p2 + 2));
        p->https_strrep_aim_len = strlen(p->https_strrep_aim);
        p->https_strrep_obj_len = strlen(p->https_strrep_obj);
    }

    // https regrep
    if (iniparser_find_entry(ini, "https:regrep") == 1) {
        p->https_regrep_len = strlen(iniparser_getstring(ini, "https:regrep", NULL)) + 1;
        p->https_regrep = (char *)malloc(p->https_regrep_len);
        if (p->https_regrep == NULL)
            free(p->https_regrep);
        memset(p->https_regrep, 0, p->https_regrep_len);
        memcpy(p->https_regrep, iniparser_getstring(ini, "https:regrep", NULL), p->https_regrep_len);
        char *p4 = strstr(p->https_regrep, "->");
        p->https_regrep_aim = (char *)malloc(strlen(p->https_regrep) - strlen(p4 + 2) - 2 + 1);
        if (p->https_regrep_aim == NULL)
            free(p->https_regrep_aim);
        strncpy_(p->https_regrep_aim, p->https_regrep, strlen(p->https_regrep) - strlen(p4 + 2) - 2);
        p->https_regrep_obj = (char *)malloc(strlen(p4 + 2) + 1);
        if (p->https_regrep_obj == NULL)
            free(p->https_regrep_obj);
        strncpy_(p->https_regrep_obj, p4 + 2, strlen(p4 + 2));
        p->https_regrep_aim_len = strlen(p->https_regrep_aim);
        p->https_regrep_obj_len = strlen(p->https_regrep_obj);
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
    return;
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
    return;
}
