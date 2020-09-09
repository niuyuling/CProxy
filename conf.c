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

/* 在content中，设置变量(var)的首地址，值(val)的位置首地址和末地址，返回下一行指针 */
static char *set_var_val_lineEnd(char *content, char **var, char **val_begin, char **val_end)
{
    char *p, *pn, *lineEnd;
    ;
    int val_len;

    while (1) {
        if (content == NULL)
            return NULL;

        for (; *content == ' ' || *content == '\t' || *content == '\r' || *content == '\n'; content++) ;
        if (*content == '\0')
            return NULL;
        *var = content;
        pn = strchr(content, '\n');
        p = strchr(content, '=');
        if (p == NULL) {
            if (pn) {
                content = pn + 1;
                continue;
            } else
                return NULL;
        }
        content = p;
        //将变量以\0结束
        for (p--; *p == ' ' || *p == '\t'; p--) ;
        *(p + 1) = '\0';
        //值的首地址
        for (content++; *content == ' ' || *content == '\t'; content++) ;
        if (*content == '\0')
            return NULL;
        //双引号引起来的值支持换行
        if (*content == '"') {
            *val_begin = content + 1;
            *val_end = strstr(*val_begin, "\";");
            if (*val_end != NULL)
                break;
        } else
            *val_begin = content;
        *val_end = strchr(content, ';');
        if (pn && *val_end > pn) {
            content = pn + 1;
            continue;
        }
        break;
    }

    if (*val_end) {
        **val_end = '\0';
        val_len = *val_end - *val_begin;
        lineEnd = *val_end;
    } else {
        val_len = strlen(*val_begin);
        *val_end = lineEnd = *val_begin + val_len;
    }
    *val_end = *val_begin + val_len;
    //printf("var[%s]\nbegin[%s]\n\n", *var, *val_begin);
    return lineEnd;
}

/* 在buff中读取模块(global http https httpdns httpudp)内容 */
static char *read_module(char *buff, const char *module_name)
{
    int len;
    char *p, *p0;

    len = strlen(module_name);
    p = buff;
    while (1) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
            p++;
        if (strncasecmp(p, module_name, len) == 0) {
            p += len;
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
                p++;
            if (*p == '{')
                break;
        }
        if ((p = strchr(p, '\n')) == NULL)
            return NULL;
    }
    if ((p0 = strchr(++p, '}')) == NULL)
        return NULL;

    return strndup(p, p0 - p);
}

static void parse_global_module(char *content, conf * p)
{
    char *var, *val_begin, *val_end, *lineEnd;

    while ((lineEnd = set_var_val_lineEnd(content, &var, &val_begin, &val_end)) != NULL) {
        if (strcasecmp(var, "uid") == 0) {
            p->uid = atoi(val_begin);
        } else if (strcasecmp(var, "process") == 0) {
            p->process = atoi(val_begin);
        } else if (strcasecmp(var, "timeout") == 0) {
            p->timeout = atoi(val_begin);
        } else if (strcasecmp(var, "encode") == 0) {
            p->sslencoding = atoi(val_begin);
        } else if (strcasecmp(var, "tcp_listen") == 0) {
            p->tcp_listen = atoi(val_begin);
        } else if (strcasecmp(var, "dns_listen") == 0) {
            p->dns_listen = atoi(val_begin);
        }

        content = strchr(lineEnd + 1, '\n');
    }
}

static void parse_http_module(char *content, conf * p)
{
    char *var, *val_begin, *val_end, *lineEnd;
    int val_begin_len;

    while ((lineEnd = set_var_val_lineEnd(content, &var, &val_begin, &val_end)) != NULL) {
        if (strcasecmp(var, "http_ip") == 0) {
            val_begin_len = strlen(val_begin) + 1;
            p->http_ip = (char *)malloc(val_begin_len);
            memset(p->http_ip, 0, val_begin_len);
            memcpy(p->http_ip, val_begin, val_begin_len);
        } else if (strcasecmp(var, "http_port") == 0) {
            p->http_port = atoi(val_begin);
        } else if (strcasecmp(var, "http_del") == 0) {
            val_begin_len = strlen(val_begin) + 1;
            p->http_del = (char *)malloc(val_begin_len);
            memset(p->http_del, 0, val_begin_len);
            memcpy(p->http_del, val_begin, val_begin_len);
            p->http_del_len = val_begin_len;
        } else if (strcasecmp(var, "http_first") == 0) {
            val_begin_len = strlen(val_begin) + 1;
            p->http_first = (char *)malloc(val_begin_len);
            memset(p->http_first, 0, val_begin_len);
            memcpy(p->http_first, val_begin, val_begin_len);
            p->http_first_len = val_begin_len;
        } else if (strcasecmp(var, "strrep") == 0) {
            val_begin_len = strlen(val_begin) + 1;

            p->http_strrep = (char *)malloc(val_begin_len);
            if (p->http_strrep == NULL)
                free(p->http_strrep);
            memset(p->http_strrep, 0, val_begin_len);
            memcpy(p->http_strrep, val_begin, val_begin_len);

            char *p1 = strstr(val_begin, "->");
            p->http_strrep_aim = (char *)malloc(val_begin_len - strlen(p1 + 2) - 2 + 1);
            if (p->http_strrep_aim == NULL) {
                free(p->http_strrep_aim);
            }
            memset(p->http_strrep_aim, 0, val_begin_len - strlen(p1 + 2) - 2 + 1);
            strncpy_(p->http_strrep_aim, val_begin, val_begin_len - strlen(p1 + 2) - 3); // 实际 val_begin_len 多1
            p->http_strrep_obj = (char *)malloc(strlen(p1 + 2) + 1);
            if (p->http_strrep_obj == NULL) {
                free(p->http_strrep_obj);
            }
            memset(p->http_strrep_obj, 0, strlen(p1 + 2) + 1);
            strncpy_(p->http_strrep_obj, p1 + 2, strlen(p1 + 2));
            p->http_strrep_aim_len = strlen(p->http_strrep_aim);
            p->http_strrep_obj_len = strlen(p->http_strrep_obj);
        } else if (strcasecmp(var, "regrep") == 0) {
            val_begin_len = strlen(val_begin) + 1;

            p->http_regrep = (char *)malloc(val_begin_len);
            if (p->http_regrep == NULL)
                free(p->http_regrep);
            memset(p->http_regrep, 0, val_begin_len);
            memcpy(p->http_regrep, val_begin, val_begin_len);

            char *p1 = strstr(val_begin, "->");
            p->http_regrep_aim = (char *)malloc(val_begin_len - strlen(p1 + 2) - 2 + 1);
            if (p->http_regrep_aim == NULL) {
                free(p->http_regrep_aim);
            }
            memset(p->http_regrep_aim, 0, val_begin_len - strlen(p1 + 2) - 2 + 1);
            strncpy_(p->http_regrep_aim, val_begin, val_begin_len - strlen(p1 + 2) - 3);
            p->http_regrep_obj = (char *)malloc(strlen(p1 + 2) + 1);
            if (p->http_regrep_obj == NULL) {
                free(p->http_regrep_obj);
            }
            memset(p->http_regrep_obj, 0, strlen(p1 + 2) + 1);
            strncpy_(p->http_regrep_obj, p1 + 2, strlen(p1 + 2));
            p->http_regrep_aim_len = strlen(p->http_regrep_aim);
            p->http_regrep_obj_len = strlen(p->http_regrep_obj);
        }

        content = strchr(lineEnd + 1, '\n');
    }
}

static void parse_https_module(char *content, conf * p)
{
    char *var, *val_begin, *val_end, *lineEnd;
    int val_begin_len;

    while ((lineEnd = set_var_val_lineEnd(content, &var, &val_begin, &val_end)) != NULL) {
        if (strcasecmp(var, "https_ip") == 0) {
            val_begin_len = strlen(val_begin) + 1;
            p->https_ip = (char *)malloc(val_begin_len);
            memset(p->https_ip, 0, val_begin_len);
            memcpy(p->https_ip, val_begin, val_begin_len);
        } else if (strcasecmp(var, "https_port") == 0) {
            p->https_port = atoi(val_begin);
        } else if (strcasecmp(var, "https_del") == 0) {
            val_begin_len = strlen(val_begin) + 1;
            p->https_del = (char *)malloc(val_begin_len);
            memset(p->https_del, 0, val_begin_len);
            memcpy(p->https_del, val_begin, val_begin_len);
            p->https_del_len = val_begin_len;
        } else if (strcasecmp(var, "https_first") == 0) {
            val_begin_len = strlen(val_begin) + 1;
            p->https_first = (char *)malloc(val_begin_len);
            memset(p->https_first, 0, val_begin_len);
            memcpy(p->https_first, val_begin, val_begin_len);
            p->https_first_len = val_begin_len;
        } else if (strcasecmp(var, "strrep") == 0) {
            val_begin_len = strlen(val_begin) + 1;

            p->https_strrep = (char *)malloc(val_begin_len);
            if (p->https_strrep == NULL)
                free(p->https_strrep);
            memset(p->https_strrep, 0, val_begin_len);
            memcpy(p->https_strrep, val_begin, val_begin_len);

            char *p1 = strstr(val_begin, "->");
            p->https_strrep_aim = (char *)malloc(val_begin_len - strlen(p1 + 2) - 2 + 1);
            if (p->https_strrep_aim == NULL) {
                free(p->https_strrep_aim);
            }
            memset(p->https_strrep_aim, 0, val_begin_len - strlen(p1 + 2) - 2 + 1);
            strncpy_(p->https_strrep_aim, val_begin, val_begin_len - strlen(p1 + 2) - 3);
            p->https_strrep_obj = (char *)malloc(strlen(p1 + 2) + 1);
            if (p->https_strrep_obj == NULL) {
                free(p->https_strrep_obj);
            }
            memset(p->https_strrep_obj, 0, strlen(p1 + 2) + 1);
            strncpy_(p->https_strrep_obj, p1 + 2, strlen(p1 + 2));
            p->https_strrep_aim_len = strlen(p->https_strrep_aim);
            p->https_strrep_obj_len = strlen(p->https_strrep_obj);
        } else if (strcasecmp(var, "regrep") == 0) {
            val_begin_len = strlen(val_begin) + 1;

            p->https_regrep = (char *)malloc(val_begin_len);
            if (p->https_regrep == NULL)
                free(p->https_regrep);
            memset(p->https_regrep, 0, val_begin_len);
            memcpy(p->https_regrep, val_begin, val_begin_len);

            char *p1 = strstr(val_begin, "->");
            p->https_regrep_aim = (char *)malloc(val_begin_len - strlen(p1 + 2) - 2 + 1);
            if (p->https_regrep_aim == NULL)
                free(p->https_regrep_aim);
            memset(p->https_regrep_aim, 0, val_begin_len - strlen(p1 + 2) - 2 + 1);
            strncpy_(p->https_regrep_aim, val_begin, val_begin_len - strlen(p1 + 2) - 3);
            p->https_regrep_obj = (char *)malloc(strlen(p1 + 2) + 1);
            if (p->https_regrep_obj == NULL)
                free(p->https_regrep_obj);
            memset(p->https_regrep_obj, 0, strlen(p1 + 2) + 1);
            strncpy_(p->https_regrep_obj, p1 + 2, strlen(p1 + 2));
            p->https_regrep_aim_len = strlen(p->https_regrep_aim);
            p->https_regrep_obj_len = strlen(p->https_regrep_obj);
        }

        content = strchr(lineEnd + 1, '\n');
    }
}

static void parse_httpdns_module(char *content, conf * p)
{
    char *var, *val_begin, *val_end, *lineEnd;
    int val_begin_len;

    while ((lineEnd = set_var_val_lineEnd(content, &var, &val_begin, &val_end)) != NULL) {
        if (strcasecmp(var, "addr") == 0) {
            val_begin_len = strlen(val_begin) + 1;
            p->addr = (char *)malloc(val_begin_len);
            memset(p->addr, 0, val_begin_len);
            memcpy(p->addr, val_begin, val_begin_len);
        } else if (strcasecmp(var, "http_req") == 0) {
            val_begin_len = strlen(val_begin) + 1;
            p->http_req = (char *)malloc(val_begin_len);
            memset(p->http_req, 0, val_begin_len);
            memcpy(p->http_req, val_begin, val_begin_len);
            p->http_req_len = val_begin_len;
        } else if (strcasecmp(var, "encode") == 0) {
            p->encode = atoi(val_begin);
        }
        content = strchr(lineEnd + 1, '\n');
    }
}

void free_conf(conf * p)
{
    if (p->http_ip)
        free(p->http_ip);
    if (p->http_del)
        free(p->http_del);
    if (p->http_first)
        free(p->http_first);
    if (p->http_strrep)
        free(p->http_strrep);
    if (p->http_strrep_aim)
        free(p->http_strrep_aim);
    if (p->http_strrep_obj)
        free(p->http_strrep_obj);
    if (p->http_regrep)
        free(p->http_regrep);
    if (p->http_regrep_aim)
        free(p->http_regrep_aim);
    if (p->http_regrep_obj)
        free(p->http_regrep_obj);

    if (p->https_ip)
        free(p->https_ip);
    if (p->https_del)
        free(p->https_del);
    if (p->https_first)
        free(p->https_first);
    if (p->https_strrep)
        free(p->https_strrep);
    if (p->https_strrep_aim)
        free(p->https_strrep_aim);
    if (p->https_strrep_obj)
        free(p->https_strrep_obj);
    if (p->https_regrep)
        free(p->https_regrep);
    if (p->https_regrep_aim)
        free(p->https_regrep_aim);
    if (p->https_regrep_obj)
        free(p->https_regrep_obj);
    
    if (p->addr)
        free(p->addr);
    if (p->http_req)
        free(p->http_req);
    return;
}

void read_conf(char *filename, conf * configure)
{
    char *buff, *global_content, *http_content, *https_content, *httpdns_content;
    FILE *file;
    long file_size;

    file = fopen(filename, "r");
    if (file == NULL) {
        perror("cannot open config file.");
        exit(-1);
    }
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    buff = (char *)alloca(file_size + 1);
    if (buff == NULL)
        perror("out of memory.");
    rewind(file);
    if (fread(buff, file_size, 1, file) < 1) {
        perror("fread");
    }
    fclose(file);
    buff[file_size] = '\0';

    if ((global_content = read_module(buff, "global")) == NULL)
        perror("read global module error");
    parse_global_module(global_content, configure);
    free(global_content);

    if ((http_content = read_module(buff, "http")) == NULL)
        perror("read http module error");
    parse_http_module(http_content, configure);
    free(http_content);

    if ((https_content = read_module(buff, "https")) == NULL)
        perror("read https module error");
    parse_https_module(https_content, configure);
    free(https_content);
    
    if ((httpdns_content = read_module(buff, "httpdns")) == NULL)
        perror("read httpdns module error");
    parse_httpdns_module(httpdns_content, configure);
    free(httpdns_content);
}
