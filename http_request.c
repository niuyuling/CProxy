#include "http_request.h"

void errors(const char *error_info)
{
    fprintf(stderr, "%s\n\n", error_info);
    exit(1);
}

int8_t copy_new_mem(char *src, int src_len, char **dest)
{
    *dest = (char *)malloc(src_len + 1);
    if (*dest == NULL)
        return 1;
    memcpy(*dest, src, src_len);
    *((*dest) + src_len) = '\0';

    return 0;
}

// 字符串替换
char *replace(char *replace_memory, int *replace_memory_len, const char *src, const int src_len, const char *dest, const int dest_len)
{
    if (!replace_memory || !src || !dest)
        return replace_memory;

    char *p;
    int diff;

    if (src_len == dest_len) {
        for (p = memmem(replace_memory, *replace_memory_len, src, src_len); p; p = memmem(p, *replace_memory_len - (p - replace_memory), src, src_len)) {
            memcpy(p, dest, dest_len);
            p += dest_len;
        }
    } else if (src_len < dest_len) {
        int before_len;
        char *before_end, *new_replace_memory;

        diff = dest_len - src_len;
        for (p = memmem(replace_memory, *replace_memory_len, src, src_len); p; p = memmem(p, *replace_memory_len - (p - replace_memory), src, src_len)) {
            *replace_memory_len += diff;
            before_len = p - replace_memory;
            new_replace_memory = (char *)realloc(replace_memory, *replace_memory_len + 1);
            if (new_replace_memory == NULL) {
                free(replace_memory);
                return NULL;
            }
            replace_memory = new_replace_memory;
            before_end = replace_memory + before_len;
            p = before_end + dest_len;
            memmove(p, p - diff, *replace_memory_len - (p - replace_memory));
            memcpy(before_end, dest, dest_len);
        }
    } else if (src_len > dest_len) {
        diff = src_len - dest_len;
        for (p = memmem(replace_memory, *replace_memory_len, src, src_len); p; p = memmem(p, *replace_memory_len - (p - replace_memory), src, src_len)) {
            *replace_memory_len -= diff;
            memcpy(p, dest, dest_len);
            p += dest_len;
            memmove(p, p + diff, *replace_memory_len - (p - replace_memory));
        }
    }

    replace_memory[*replace_memory_len] = '\0';
    return replace_memory;
}

/* 正则表达式字符串替换，str为可用free释放的指针 */
static char *regrep(char *str, int *str_len, const char *src, char *dest, int dest_len)
{
    if (!str || !src || !dest)
        return NULL;

    regmatch_t pm[10];
    regex_t reg;
    char child_num[2] = { '\\', '0' }, *p, *real_dest;
    int match_len, real_dest_len, i;

    p = str;
    regcomp(&reg, src, REG_NEWLINE | REG_ICASE | REG_EXTENDED);
    while (regexec(&reg, p, 10, pm, 0) == 0) {
        real_dest = (char *)malloc(dest_len);
        if (real_dest == NULL) {
            regfree(&reg);
            free(str);
            return NULL;
        }
        memcpy(real_dest, dest, dest_len);
        real_dest_len = dest_len;
        //不进行不必要的字符串操作
        if (pm[1].rm_so >= 0) {
            /* 替换目标字符串中的子表达式 */
            for (i = 1; i < 10 && pm[i].rm_so > -1; i++) {
                child_num[1] = i + 48;
                real_dest = replace(real_dest, &real_dest_len, child_num, 2, p + pm[i].rm_so, pm[i].rm_eo - pm[i].rm_so);
                if (real_dest == NULL) {
                    regfree(&reg);
                    free(str);
                    return NULL;
                }
            }
        }

        match_len = pm[0].rm_eo - pm[0].rm_so;
        p += pm[0].rm_so;
        //目标字符串不大于匹配字符串则不用分配新内存
        if (match_len >= real_dest_len) {
            memcpy(p, real_dest, real_dest_len);
            if (match_len > real_dest_len)
                //strcpy(p + real_dest_len, p + match_len);
                memmove(p + real_dest_len, p + match_len, *str_len - (p + match_len - str));
            p += real_dest_len;
            *str_len -= match_len - real_dest_len;
        } else {
            int diff;
            char *before_end, *new_str;

            diff = real_dest_len - match_len;
            *str_len += diff;
            new_str = (char *)realloc(str, *str_len + 1);
            if (new_str == NULL) {
                free(str);
                free(real_dest);
                regfree(&reg);
                return NULL;
            }
            str = new_str;
            before_end = str + pm[0].rm_so;
            p = before_end + real_dest_len;
            memmove(p, p - diff, *str_len - (p - str) + 1);
            memcpy(before_end, real_dest, real_dest_len);
        }
        free(real_dest);
    }

    regfree(&reg);
    return str;
}

int extract_host(char *header, char *host, char *port)
{
    memset(port, 0, strlen(port));
    memset(host, 0, strlen(host));
//printf("%s\n", header);
    char *_p = strstr(header, "CONNECT"); // 在 CONNECT 方法中解析 隧道主机名称及端口号
    if (_p)
    {

        if (strchr(header, '[') || strchr(header, ']')) { // IPv6
            char *_p1 = strchr(header, '[');
            char *_p2 = strchr(_p1 + 1, ']');
            strncpy(host, _p1 + 1, (int)(_p2 - _p1) - 1);

            char *_p3 = strchr(_p2 + 1, ' ');
            strncpy(port, _p2 + 2, (int)(_p3 - _p2) - 1);

            return 0;
        }

        char *_p1 = strchr(_p, ' ');
        char *_p2 = strchr(_p1 + 1, ':');
        char *_p3 = strchr(_p1 + 1, ' ');

        if (_p2) {
            memcpy(host, _p1 + 1, (int)(_p2 - _p1) - 1);
            memcpy(port, _p2 + 1, (int)(_p3 - _p2) - 1);
        } else {                // 如果_p2等于空就返回-1
            return -1;
        }
        
        return 0;
    } else {

        char *_p = strstr(header, "Host:");
        if (_p == NULL) {
            _p = strstr(header, "host:");
        }
        if (_p == NULL) {                // 都为空时
            return -1;
        }
        
        char *_p1 = strchr(_p, '\n');     // 指向末尾'\n'
        if (!_p1) {
            return -1;
        }

        char *_p2 = strchr(_p + 5, ':'); // 5是指'Host:'的长度
        int h_len = (int)(_p1 - _p - 6);
        char s_host[h_len];
        strncpy(s_host, _p + 6, _p1 - _p - 6);
        s_host[h_len] = '\0';
        char *_p3 = strchr(s_host, ':');
        char *_p4 = NULL;
        if (_p3)
            _p4 = strchr(_p3 + 1, ':');
        {                                               // IPV6
            if (_p4 != NULL) {
                char *_p5 = NULL;
                char *_p6 = NULL;
                _p5 = strchr(header, ' ');
                if (_p5)
                    _p6 = strchr(_p5 + 1, ' ');

                char url[_p6 - _p5 - 1];
                memset(url, 0, _p6 - _p5 - 1);
                strncpy(url, _p5 + 1, _p6 - _p5 - 1);
                url[_p6 - _p5 - 1] = '\0';

                if (strstr(url, "http") != NULL) {      // 去除 'http://'
                    memcpy(url, url + 7, (_p6 - _p5 - 1) - 7);
                    url[(_p6 - _p5 - 1) - 7] = '\0';
                    char *_p7 = strchr(url, '/');
                    if (_p7)         // 去除 uri
                        url[_p7 - url] = '\0';

                    char *_p8 = strchr(url, ']');
                    char *_p9 = strchr(url, '\0');
                    if (_p8) {
                        strcpy(port, _p8 + 2);
                        strncpy(host, url + 1, _p8 - (url+1));

                        if ((_p9-_p8) == 1) {
                            strcpy(port, "80");
                            strncpy(host, url + 1, _p8 - (url+1));
                        }
                    }
                    return 0;
                } else {            // HTTP头为不规范的url时处理Host, 主要Proxifier转发url为'/'时
                    //printf("s_host: %s\n", s_host);
                    char *_p1 = strchr(s_host, '[');
                    if (_p1 == NULL)                     // 涉及到自定义的Host, 不带'['、']'时, 默认截取最后为端口
                    {
                        char *_p2 = strrchr(s_host, ':');
                        remote_port = atoi(_p2+1);
                        strncpy(remote_host, s_host, _p2-s_host);
                        return 0;
                    }
                    
                    char *_p2 = strchr(_p1 + 1, ']');
                    if (_p1 && _p2) {
                        memcpy(host, _p1 + 1, _p2 - _p1 - 1);
                        if (strlen(_p2) < 3) {
                            strcpy(port, "80");
                        } else {
                            strcpy(port, _p2 + 2);
                        }

                    }
                    return 0;
                }

                return -1;
            }
        }
        // HTTP 非 CONNECT 方法
        {
            if (_p2 && _p2 < _p1) {        // 带端口, p2 指向':' p1 指向末尾'\n'
                memcpy(port, _p2 + 1, (int)(_p1 - _p2 - 1));
                memcpy(host, _p + 5 + 1, (int)(_p2 - _p - 5 - 1));
            } else {                    // 不带端口
                if (0 < (int)(_p1 - _p - 5 - 1 - 1)) {
                    memcpy(host, _p + 5 + 1, (_p1 - _p - 5 - 1 - 1));
                    memcpy(port, "80", 2);
                } else {
                    memcpy(host, _p + 5 + 1, (_p1 - _p) - 6);
                    memcpy(port, "80", 2);
                }
            }
        }

        return 0;
    }

    return 0;
}

char *get_http_path(char *url, char *path)
{
    char *_p0;
    _p0 = NULL;
    int url_len;
    url_len = 0;
    url_len = strlen(url);

    if (url_len > 7) {
        if (url) {
            _p0 = strstr(url + 7, "/");
            if (_p0)
                return memcpy(path, _p0, (int)strlen(_p0));
            else
                memcpy(path, "/", 1); // 如果没有资源路径就默认"/"
        }
    } else {
        memcpy(path, "/", 1);
    }

    return NULL;
}

void get_http_host_port_len(char *head, int *host_len, int *port_len)
{
    *host_len = 0;
    *port_len = 0;
    char *_p1 = strstr(head, "Host"); // 判断Host行
    if (_p1) {                  // 为真时
        char *_p2 = strstr(_p1, "\n");
        *host_len = (int)(_p2 - _p1);

        char host[*host_len + 1];
        memcpy(host, _p1, *host_len);
        host[*host_len] = '\0';

        char *_p3 = strrchr(host, ':');
        if (_p3) {
            *port_len = strlen(_p3 + 1);
        } else {
            *port_len = *host_len;
        }
    } else {                    // 为假时
        char *_p1 = strstr(head, "host");
        if (_p1) {
            char *_p2 = strstr(_p1, "\n");
            *host_len = (int)(_p2 - _p1);

            char host[*host_len + 1];
            memcpy(host, _p1, *host_len);
            host[*host_len] = '\0';

            char *_p3 = strrchr(host, ':');
            if (_p3) {
                *port_len = strlen(_p3 + 1);
            } else {
                *port_len = *host_len;
            }
        } else {                // 未找到时使用HTTP_HEAD_CACHE_SIZE大小
            *host_len = HTTP_HEAD_HOST_CACHE_SIZE;
            *port_len = HTTP_HEAD_HOST_CACHE_SIZE/10;
        }
    }

    return;
}

void free_http_request(struct http_request *http_request)
{
    if (http_request->method)
        free(http_request->method);
    if (http_request->U)
        free(http_request->U);
    if (http_request->version)
        free(http_request->version);
    if (http_request->host)
        free(http_request->host);
    if (http_request->port)
        free(http_request->port);
    if (http_request->H)
        free(http_request->H);
    if (http_request->url)
        free(http_request->url);
    if (http_request->uri)
        free(http_request->uri);
}

void parse_request_head(char *http_request_line, struct http_request *http_request)
{
    char *p, *head, *m, *u;
    size_t head_len;
    int host_len, port_len, uri_len;
    host_len = port_len = uri_len = 0;

    p = strstr(http_request_line, "\r\n"); // 查找"\r\n"
    if (p == NULL) {
        return;
    }

    head_len = p - http_request_line;
    head = (char *)malloc(sizeof(char) * head_len + 1);
    if (head == NULL)
        free(head);
    memset(head, 0, sizeof(char) * head_len + 1);
    memcpy(head, http_request_line, head_len);

    http_request->method = (char *)malloc(sizeof(char) * 8);
    http_request->U = (char *)malloc(sizeof(char) * head_len);
    http_request->version = (char *)malloc(10);
    if (http_request->method == NULL) {
        perror("malloc");
    }
    if (http_request->U == NULL) {
        perror("malloc");
    }
    if (http_request->version == NULL) {
        perror("malloc");
    }
    memset(http_request->method, 0, 8);
    memset(http_request->U, 0, sizeof(char) * head_len);
    memset(http_request->version, 0, 10);

    m = strchr(head, ' ');
    http_request->method_len = m - head;
    memmove(http_request->method, head, http_request->method_len);
    
    u = strchr(m + 1, ' ');
    memmove(http_request->U, m + 1, u - (m+1));
    
    memmove(http_request->version, u + 1, 8);
    http_request->version_len = 8;
    http_request->U_len = (int)strlen(http_request->U);

    // 获取Host、Port长度
    get_http_host_port_len(http_request_line, &host_len, &port_len);

    // URI LENGTH
    char *_p0 = strstr(http_request->U, "http://");
    if (_p0) {                  // 标准头
        char *_p1 = strchr(http_request->U + 7, '/');
        if (_p1) {
            uri_len = (int)strlen(_p1);
        }
    } else {                    // 非标准头
        char *_p1 = strchr(http_request->U, '/');
        if (_p1) {
            uri_len = (int)strlen(_p1);
        } else {
            uri_len = 1;        // 没有uri时
        }
    }

    http_request->host = (char *)malloc(sizeof(char) * host_len + 1);
    if (http_request->host == NULL)
        perror("malloc");
    http_request->port = (char *)malloc(sizeof(char) * port_len + 1);
    if (http_request->port == NULL)
        perror("malloc");
    http_request->url = (char *)malloc(sizeof(char) * http_request->U_len + 1);
    if (http_request->url == NULL)
        perror("malloc");
    http_request->uri = (char *)malloc(sizeof(char) * uri_len + 1);
    if (http_request->uri == NULL)
        perror("malloc");
    http_request->H = (char *)malloc(sizeof(char) * host_len + port_len + 1);
    if (http_request->H == NULL)
        perror("malloc");

    memset(http_request->host, 0, host_len + 1);
    memset(http_request->port, 0, port_len + 1);
    memset(http_request->url, 0, http_request->U_len + 1);
    memset(http_request->uri, 0, uri_len + 1);
    memset(http_request->H, 0, host_len + port_len + 1);

    if (extract_host(http_request_line, http_request->host, http_request->port) == -1)
        return;
    http_request->host_len = (int)strlen(http_request->host);
    http_request->port_len = (int)strlen(http_request->port);
    memcpy(http_request->H, http_request->host, http_request->host_len);
    strcat(http_request->H, ":");
    strcat(http_request->H, http_request->port);
    memcpy(http_request->url, http_request->U, http_request->U_len);
    get_http_path(http_request->url, http_request->uri);

    http_request->U_len = (int)strlen(http_request->U);
    http_request->url_len = (int)strlen(http_request->url);
    http_request->uri_len = (int)strlen(http_request->uri);
    http_request->H_len = (int)strlen(http_request->H);
    /*
    printf("%s %d\n", http_request->method, http_request->method_len);
    printf("%s %d\n", http_request->U, http_request->U_len);
    printf("%s %d\n", http_request->version, http_request->version_len);
    printf("%s %d\n", http_request->host, http_request->host_len);
    printf("%s %d\n", http_request->port, http_request->port_len);
    printf("%s %d\n", http_request->H, http_request->H_len);
    //printf("%s %d\n", http_request->url);
    //printf("%s %d\n", http_request->uri);
    */

    free(head);
    return;
}

static char *splice_head(char *head, const char *needle, char *string)
{
    char *tail_head;
    char *_p0;
    char *_p1;
    
    _p1 = strstr(head, needle);
    if (_p1 == NULL) {
        return head;
    }
    _p1 = _p1 + 1;
    _p0 = strchr(_p1, '\0');
    
    tail_head = (char *)alloca((_p0 - _p1) + 1);
    if (tail_head == NULL) {
        perror("alloca");
        return head;
    }
    memset(tail_head, 0, (_p0 - _p1) + 1);
    strcpy(tail_head, _p1);

    memset(head, 0, strlen(head));
    strcpy(head, string);
    strcat(head, tail_head);
    
    return head;
}

static char *delete_head(char *head, const char *needle, int string)
{
    char *temp_stack;
    char *_p1, *_p2, *_p3;
    _p1 = _p2 = _p3 = NULL;
    int temp_stack_len;

    _p1 = strstr(head, needle);     // _p1指向head字符串中的"needle"字符处(needle字符串的第一个字符)
    if (_p1 == NULL) {
        //perror("_p1 HEAD NULL");
        return head;
    }
    _p2 = strchr(_p1, string);      // _p2指向head字符串中"string"字符到末尾中的'\0'
    if (_p2 == NULL) {
        //perror("_p2 HEAD NULL");
        return head;
    }
    _p3 = strchr(_p1, '\0');        // _p3指向head字符串末尾的'\0'
    if (_p3 == NULL) {
        //perror("_p3 HEAD NULL");
        return head;
    }
    temp_stack_len = (_p1 - head) + (_p3 - _p2);
    temp_stack = (char *)alloca(temp_stack_len + 1);     // 分配临时栈内存，长度是去除needle到string处
    if (temp_stack == NULL) {
        perror("alloca");
        return head;
    }
    memset(temp_stack, 0, temp_stack_len + 1);
    memmove(temp_stack, head, (_p1 - head) - 1);
    strcat(temp_stack, _p2);

    
    memset(head, 0, strlen(head));
    return memmove(head, temp_stack, temp_stack_len);
}

static char *conf_handle_strrep(char *str, int str_len, tcp *temp)
{
    tcp *p = temp;
    while (p) {
        if (p->strrep) {
            str = replace(str, &str_len, p->strrep_s, p->strrep_s_len, p->strrep_t, p->strrep_t_len);
        }

        p = p->next;
    }

    return str;
}

static char *conf_handle_regrep(char *str, int str_len, tcp *temp)
{
    tcp *p = temp;
    while (p) {
        if (p->regrep) {
            str = regrep(str, &str_len, p->regrep_s, p->regrep_t, p->regrep_t_len);
        }

        p = p->next;
    }

    return str;
}

char *request_head(conn_t * in, conf * configure)
{
    char *result = NULL;
    char *delim = ",";
    char *saveptr = NULL;
    char *incomplete_head = NULL;
    int incomplete_head_len = 0;
    int return_val = 0;
    struct http_request *http_request;
    http_request = (struct http_request *)malloc(sizeof(struct http_request));
    memset(http_request, 0, sizeof(struct http_request));

    parse_request_head(in->incomplete_data, http_request);

    if ((return_val = strncmp(in->incomplete_data, "CONNECT", 7)) == 0)
    {
        char https_del_copy[configure->https_del_len+1];
        
        memset(remote_host, 0, CACHE_SIZE);
        if (configure->https_port > 0) {
            remote_port = configure->https_port;
        }
        if (configure->https_ip != NULL) {
            strcpy(remote_host, configure->https_ip);
        }
        incomplete_head = (char *)malloc(sizeof(char) * (BUFFER_SIZE));
        if (incomplete_head == NULL) {
            free(incomplete_head);
            perror("malloc");
        }
        memset(incomplete_head, 0, sizeof(char) * (BUFFER_SIZE));
        memmove(incomplete_head, in->incomplete_data, in->incomplete_data_len);
        memmove(https_del_copy, configure->https_del, configure->https_del_len+1);

        
        result = strtok_r(https_del_copy, delim, &saveptr);
        while (result != NULL) {
            delete_head(incomplete_head, result, '\n');
            result = strtok_r(NULL, delim, &saveptr);
        }
        
        splice_head(incomplete_head, "\n", configure->https_first);
        incomplete_head_len = strlen(incomplete_head);          // 更新HTTPS HEADER长度
        
        incomplete_head = conf_handle_strrep(incomplete_head, incomplete_head_len, https_head_strrep);
        incomplete_head_len = strlen(incomplete_head) + 1;      // 更新HTTPS HEADER长度
        incomplete_head = conf_handle_regrep(incomplete_head, incomplete_head_len, https_head_regrep);
        
        incomplete_head_len = strlen(incomplete_head);          // 更新HTTPS HEADER长度
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[M]", 3, http_request->method, http_request->method_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[method]", 8, http_request->method, http_request->method_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[U]", 3, http_request->U, http_request->U_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[V]", 3, http_request->version, http_request->version_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[version]", 9, http_request->version, http_request->version_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[host]", 6, http_request->host, http_request->host_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[port]", 6, http_request->port, http_request->port_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[H]", 3, http_request->H, http_request->H_len);
        incomplete_head_len = strlen(incomplete_head);      // 更新HTTPS HEADER长度
        //printf("%s", incomplete_head);                      // 打印HTTPS HEADER

        char *new_incomplete_data;
        new_incomplete_data = (char *)realloc(in->incomplete_data, incomplete_head_len + 1);
        if (new_incomplete_data == NULL) {
            free(in->incomplete_data);
            perror("realloc");
            return in->incomplete_data;
        }
        in->incomplete_data = new_incomplete_data;
        memset(in->incomplete_data, 0, incomplete_head_len + 1);                // 清空incomplete_data数据
        memmove(in->incomplete_data, incomplete_head, incomplete_head_len);     // 更新incomplete_data数据
        in->incomplete_data_len = incomplete_head_len;                          // 更新incomplete_data长度

        free(incomplete_head);                                                  // 释放incomplete_head内存
    }

    if (strncmp(in->incomplete_data, "GET", 3) == 0 || strncmp(in->incomplete_data, "POST", 4) == 0)
    {
        char http_del_copy[configure->http_del_len + 1];

        memset(remote_host, 0, CACHE_SIZE);
        if (configure->http_port > 0)
            remote_port = configure->http_port;
        if (configure->http_ip != NULL)
            strcpy(remote_host, configure->http_ip);
        incomplete_head = (char *)malloc(sizeof(char) * (BUFFER_SIZE));
        if (incomplete_head == NULL) {
            perror("malloc");
            free(incomplete_head);
        }

        memset(incomplete_head, 0, sizeof(char) * (BUFFER_SIZE));
        memmove(incomplete_head, in->incomplete_data, in->incomplete_data_len);
        memmove(http_del_copy, configure->http_del, configure->http_del_len+1);

        result = strtok_r(http_del_copy, delim, &saveptr);
        while (result != NULL) {
            delete_head(incomplete_head, result, '\n');
            result = strtok_r(NULL, delim, &saveptr);
        }

        splice_head(incomplete_head, "\n", configure->http_first);
        incomplete_head_len = strlen(incomplete_head);          // 更新HTTP HEADER长度
        incomplete_head = conf_handle_strrep(incomplete_head, incomplete_head_len, http_head_strrep);
        incomplete_head_len = strlen(incomplete_head) + 1;      // 更新HTTP HEADER长度
        incomplete_head = conf_handle_regrep(incomplete_head, incomplete_head_len, http_head_regrep);
        incomplete_head_len = strlen(incomplete_head);          // 更新HTTP HEADER长度
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[M]", 3, http_request->method, http_request->method_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[method]", 8, http_request->method, http_request->method_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[U]", 3, http_request->U, http_request->U_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[V]", 3, http_request->version, http_request->version_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[version]", 9, http_request->version, http_request->version_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[url]", 5, http_request->url, http_request->url_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[uri]", 5, http_request->uri, http_request->uri_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[host]", 6, http_request->host, http_request->host_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[port]", 6, http_request->port, http_request->port_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[H]", 3, http_request->H, http_request->H_len);
        incomplete_head_len = strlen(incomplete_head);          // 更新HTTP HEADER长度
        //printf("%s", incomplete_head);                          // 打印HTTP HEADER
        
        char *new_incomplete_data;
        new_incomplete_data = (char *)realloc(in->incomplete_data, incomplete_head_len + 1);
        if (new_incomplete_data == NULL) {
            free(in->incomplete_data);
            perror("realloc");
            return in->incomplete_data;
        }
        in->incomplete_data = new_incomplete_data;
        memset(in->incomplete_data, 0, incomplete_head_len + 1);                // 清空incomplete_data数据
        memmove(in->incomplete_data, incomplete_head, incomplete_head_len);   // 更新incomplete_data数据
        in->incomplete_data_len = incomplete_head_len;                          // 更新incomplete_data长度
        
        free(incomplete_head);                                                  // 释放incomplete_head内存
    }

    free_http_request(http_request);
    free(http_request);
    return in->incomplete_data;
}
