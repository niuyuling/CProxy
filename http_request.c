#include "http_request.h"

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

// 删除字符串head中第一位到 character 处并拼接 string, character 为空返回原字符串.(string 字符替换第一个字符到 character 处)
char *splice_head(char *head, const char *character, char *string)
{
    int first_len = strlen(string);
    char *_p1 = strstr(head, character);
    if (_p1 == NULL) {
        return head;
    }
    _p1 = _p1 + 1;
    char temporary[first_len + strlen(_p1) + 1];
    memset(temporary, 0, (first_len + strlen(_p1) + 1));
    strcpy(temporary, string);
    strcat(temporary, _p1);
    memset(head, 0, strlen(head));
    return strcpy(head, temporary);
}

// 删除字符串 head 中 character 到 string 处, character 为空返回原字符串.
char *delete_head(char *head, const char *character, int string)
{
    int head_len = strlen(head);
    char *_p1 = strstr(head, character);
    if (_p1 == NULL) {
        return head;
    }
    char *_p2 = strchr(_p1, string);
    if (_p2 == NULL) {
        return head;
    }
    char temporary[head_len];
    memset(temporary, 0, head_len);
    memcpy(temporary, head, (head_len - strlen(_p1) - 1));
    strcat(temporary, _p2);
    memset(head, 0, strlen(head));
    return memcpy(head, temporary, head_len);
}

int extract_host(char *header, char *host, char *port)
{
    memset(port, 0, strlen(port));
    memset(host, 0, strlen(host));
//printf("%s\n", header);
    char *_p = strstr(header, "CONNECT"); // 在 CONNECT 方法中解析 隧道主机名称及端口号
    if (_p) {

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

        char *p = strstr(header, "Host:");
        if (!p) {
            return -1;
        }
        char *p1 = strchr(p, '\n');
        if (!p1) {
            return -1;
        }

        char *p2 = strchr(p + 5, ':');          // 5是指'Host:'的长度

        int h_len = (int)(p1 - p - 6);
        char s_host[h_len];
        strncpy(s_host, p + 6, p1 - p - 6);
        s_host[h_len] = '\0';
        char *p3 = strchr(s_host, ':');
        char *p4 = NULL;
        if (p3)
            p4 = strchr(p3 + 1, ':');

        if (p4 != NULL) {                       // IPV6
            char *p5 = NULL;
            char *p6 = NULL;
            p5 = strchr(header, ' ');
            if (p5)
                p6 = strchr(p5 + 1, ' ');

            char url[p6 - p5 - 1];
            memset(url, 0, p6 - p5 - 1);
            strncpy(url, p5 + 1, p6 - p5 - 1);
            url[p6 - p5 - 1] = '\0';
            
            if (strstr(url, "http") != NULL) {          // 去除 'http://'
                memcpy(url, url + 7, strlen(url) - 7);
                url[strlen(url) - 7] = '\0';
                char *p7 = strchr(url, '/');
                if (p7)                                 // 去除 uri
                    url[p7 - url] = '\0';
                
                char *p8 = strchr(url, ']');
                if (p8) {
                    strcpy(port, p8 + 2);
                    strncpy(host, url + 1, strlen(url) - strlen(p8) - 1);

                    if (strlen(p8) < 3) {
                        strcpy(port, "80");
                        strncpy(host, url + 1, strlen(url) - strlen(p8) - 1);
                    }
                }
                return 0;
            } else {                                   // HTTP头为不规范的url时处理Host, 主要Proxifier转发url为'/'时
                //printf("s_host: %s\n", s_host);
                char *_p1 = strchr(s_host, '[');
                char *_p2 = strchr(_p1+1, ']');
                if (_p1 && _p2) {
                    memcpy(host, _p1+1, _p2 - _p1 -1);
                    if (strlen(_p2) < 3) {
                        strcpy(port, "80");
                    } else {
                        strcpy(port, _p2+2);
                    }
                    
                }
                return 0;
            }
            
            

            return -1;
        }

        if (p2 && p2 < p1) {
            memcpy(port, p2 + 1, (int)(p1 - p2 - 1));
            memcpy(host, p + 5 + 1, (int)(p2 - p - 5 - 1));
        } else {
            memcpy(host, p + 5 + 1, (int)(p1 - p - 5 - 1 - 1));
            memcpy(port, "80", 2);
        }

        return 0;
    }

    return 0;
}

char *get_http_path(char *url, char *path)
{
    char *_p0;
    _p0 = NULL;
    if (url) {
        _p0 = strstr(url + 7, "/");
        if (_p0)
            return memcpy(path, _p0, (int)strlen(_p0));
        else
            memcpy(path, "/", 1);               // 如果没有资源路径就默认"/"
    }

    return NULL;
}

void free_http_request(struct http_request *http_request)
{
    if (http_request->M)
        free(http_request->M);
    if (http_request->U)
        free(http_request->U);
    if (http_request->V)
        free(http_request->V);
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

void get_http_host_port_len(char *head, int *host_len, int *port_len) {
    *host_len = 0;
    *port_len = 0;
    char *_p1 = strstr(head, "Host");           // 判断Host行
    if (_p1) {                                  // 为真时
        char *_p2 = strstr(_p1, "\n");
        *host_len = (int)(_p2 - _p1);
        
        char host[*host_len+1];
        memcpy(host, _p1, *host_len);
        host[*host_len] = '\0';
        
        char *_p3 = strrchr(host, ':');
        if (_p3) {
            *port_len = strlen(_p3+1);
        } else {
            *port_len = *host_len;
        }
    } else {                                    // 为假时
        char *_p1 = strstr(head, "host");
        if (_p1) {
            char *_p2 = strstr(_p1, "\n");
            *host_len = (int)(_p2 - _p1);
            
            char host[*host_len+1];
            memcpy(host, _p1, *host_len);
            host[*host_len] = '\0';
            
            char *_p3 = strrchr(host, ':');
            if (_p3) {
                *port_len = strlen(_p3+1);
            } else {
                *port_len = *host_len;
            }
        } else {                                // 未找到时使用HTTP_HEAD_CACHE_SIZE大小
            *host_len = HTTP_HEAD_CACHE_SIZE;
            *port_len = HTTP_HEAD_CACHE_SIZE;
        }
    }

    return ;
}

void parse_request_head(char *http_request_line, struct http_request *http_request)
{
    char *p;
    char *head;
    size_t head_len;
    char *m, *u;
    int host_len = 0;
    int port_len = 0;
    int uri_len = 0;

    p = strstr(http_request_line, "\r\n"); // 查找"\r\n"
    if (p == NULL) {
        return;
    }

    head_len = strlen(http_request_line) - strlen(p);
    head = (char *)malloc(sizeof(char) * head_len * 2);
    if (head == NULL)
        free(head);
    memset(head, 0, head_len * 2);
    memcpy(head, http_request_line, head_len);

    http_request->M = (char *)malloc(sizeof(char) * 7);
    http_request->U = (char *)malloc(sizeof(char) * HTTP_HEAD_CACHE_SIZE);
    http_request->V = (char *)malloc(10);
    if (http_request->M == NULL) {
        perror("malloc");
    }
    if (http_request->U == NULL) {
        perror("malloc");
    }
    if (http_request->V == NULL) {
        perror("malloc");
    }
    memset(http_request->M, 0, 7);
    memset(http_request->U, 0, HTTP_HEAD_CACHE_SIZE);
    memset(http_request->V, 0, 10);

    m = strchr(head, ' ');
    http_request->M_len = strlen(head) - strlen(m);
    //http_request->M_len = m - head;
    memcpy(http_request->M, head, http_request->M_len);
    u = strchr(m + 1, ' ');
    http_request->U_len = strlen(m + 1) - strlen(u);
    //http_request->U_len = u - m -1;
    memcpy(http_request->U, m + 1, http_request->U_len);
    memcpy(http_request->V, u + 1, 8);
    http_request->V_len = 8;
    http_request->U_len = (int)strlen(http_request->U);
    
    // 获取Host、Port长度
    get_http_host_port_len(http_request_line, &host_len, &port_len);
    
    // URI LENGTH
    char *_p0 = strstr(http_request->U, "http://");
    if (_p0) {                                      // 标准头
        char *_p1 = strchr(http_request->U + 7, '/');
        if (_p1) {
            uri_len = (int)strlen(_p1);
        }
    } else {                                        // 非标准头
        char *_p1 = strchr(http_request->U, '/');
        if (_p1) {
            uri_len = (int)strlen(_p1);
        } else {
            uri_len = 1;                            // 没有uri时
        }
    }

    http_request->host = (char *)malloc(sizeof(char) * host_len+1);
    if (http_request->host == NULL)
        perror("malloc");
    http_request->port = (char *)malloc(sizeof(char) * port_len+1);
    if (http_request->port == NULL)
        perror("malloc");
    http_request->url = (char *)malloc(sizeof(char) * http_request->U_len+1);
    if (http_request->url == NULL)
        perror("malloc");
    http_request->uri = (char *)malloc(sizeof(char) * uri_len+1);
    if (http_request->uri == NULL)
        perror("malloc");
    http_request->H = (char *)malloc(sizeof(char) * host_len + port_len + 1);
    if (http_request->H == NULL)
        perror("malloc");

    memset(http_request->host, 0, host_len+1);
    memset(http_request->port, 0, port_len+1);
    memset(http_request->url, 0, http_request->U_len+1);
    memset(http_request->uri, 0, uri_len+1);
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

    free(head);
    return;
}

char *request_head(conn_t * in, conf * configure)
{
    struct http_request *http_request;
    http_request = (struct http_request *)malloc(sizeof(struct http_request));
    memset(http_request, 0, sizeof(struct http_request));

    parse_request_head(in->incomplete_data, http_request);

    if (strncmp(in->incomplete_data, "CONNECT", 7) == 0) {
        char *incomplete_head;
        int incomplete_head_len;
        char https_del_copy[configure->https_del_len * 2];
        char *result = NULL;

        memset(remote_host, 0, CACHE_SIZE);
        if (configure->https_port > 0)
            remote_port = configure->https_port;
        if (configure->https_ip != NULL)
            strcpy(remote_host, configure->https_ip);
        incomplete_head = (char *)malloc(sizeof(char) * (BUFFER_SIZE));
        if (incomplete_head == NULL) {
            free(incomplete_head);
            perror("malloc");
        }
        memset(incomplete_head, 0, sizeof(char) * (BUFFER_SIZE));
        memcpy(incomplete_head, in->incomplete_data, strlen(in->incomplete_data));
        memcpy(https_del_copy, configure->https_del, configure->https_del_len);

        result = strtok(https_del_copy, ",");
        while (result != NULL) {
            delete_head(incomplete_head, result, '\n');
            result = strtok(NULL, ",");
        }
        splice_head(incomplete_head, "\n", configure->https_first);
        incomplete_head_len = strlen(incomplete_head);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "\\r", 2, "\r", 1);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "\\n", 2, "\n", 1);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "\\b", 2, "\b", 1);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "\\v", 2, "\v", 1);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "\\f", 2, "\f", 1);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "\\a", 2, "\a", 1);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "\\t", 2, "\t", 1);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "\\r", 2, "\r", 1);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "\\n", 2, "\n", 1);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[M]", 3, http_request->M, http_request->M_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[method]", 8, http_request->M, http_request->M_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[U]", 3, http_request->U, http_request->U_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[V]", 3, http_request->V, http_request->V_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[version]", 9, http_request->V, http_request->V_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[host]", 6, http_request->host, http_request->host_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[port]", 6, http_request->port, http_request->port_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[H]", 3, http_request->H, http_request->H_len);
        if (configure->https_strrep)
            incomplete_head = replace(incomplete_head, &incomplete_head_len, configure->https_strrep_aim, configure->https_strrep_aim_len, configure->https_strrep_obj, configure->https_strrep_obj_len);
        if (configure->https_regrep)
            incomplete_head = regrep(incomplete_head, &incomplete_head_len, configure->https_regrep_aim, configure->https_regrep_obj, configure->https_regrep_obj_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[H]", 3, http_request->H, http_request->H_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[host]", 6, http_request->host, http_request->host_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[port]", 6, http_request->port, http_request->port_len);
        incomplete_head_len = strlen(incomplete_head);      // 更新HTTPS HEADER长度
        //printf("%s", incomplete_head);                      // 打印HTTPS HEADER


        char *new_incomplete_data;
        new_incomplete_data = (char *)realloc(in->incomplete_data, incomplete_head_len + 1);        // 更新incomplete_data堆内存
        if (new_incomplete_data == NULL)    {
            perror("realloc");
            return NULL;
        }
        in->incomplete_data = new_incomplete_data;
        memset(in->incomplete_data, 0, incomplete_head_len + 1);                                    // 清空incomplete_data数据
        strcpy(in->incomplete_data, incomplete_head);                                               // 更新incomplete_data数据
        in->incomplete_data_len = strlen(in->incomplete_data);                                      // 更新incomplete_data长度
        free(incomplete_head);                                                                      // 释放incomplete_head内存

    }
    
    if (strncmp(in->incomplete_data, "GET", 3) == 0 || strncmp(in->incomplete_data, "POST", 4) == 0)    {
        char *incomplete_head;
        int incomplete_head_len;
        char http_del_copy[configure->http_del_len];
        char *result = NULL;

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
        memcpy(incomplete_head, in->incomplete_data, strlen(in->incomplete_data));
        memcpy(http_del_copy, configure->http_del, configure->http_del_len);

        result = strtok(http_del_copy, ",");
        while (result != NULL) {
            delete_head(incomplete_head, result, '\n');
            result = strtok(NULL, ",");
        }
        splice_head(incomplete_head, "\n", configure->http_first);
        incomplete_head_len = strlen(incomplete_head);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "\\r", 2, "\r", 1);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "\\n", 2, "\n", 1);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "\\b", 2, "\b", 1);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "\\v", 2, "\v", 1);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "\\f", 2, "\f", 1);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "\\a", 2, "\a", 1);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "\\t", 2, "\t", 1);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "\\r", 2, "\r", 1);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "\\n", 2, "\n", 1);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[M]", 3, http_request->M, http_request->M_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[method]", 8, http_request->M, http_request->M_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[U]", 3, http_request->U, http_request->U_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[V]", 3, http_request->V, http_request->V_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[version]", 9, http_request->V, http_request->V_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[url]", 5, http_request->url, http_request->url_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[uri]", 5, http_request->uri, http_request->uri_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[host]", 6, http_request->host, http_request->host_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[port]", 6, http_request->port, http_request->port_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[H]", 3, http_request->H, http_request->H_len);
        if (configure->http_strrep)
            incomplete_head = replace(incomplete_head, &incomplete_head_len, configure->http_strrep_aim, configure->http_strrep_aim_len, configure->http_strrep_obj, configure->http_strrep_obj_len);
        if (configure->http_regrep)
            incomplete_head = regrep(incomplete_head, &incomplete_head_len, configure->http_regrep_aim, configure->http_regrep_obj, configure->http_regrep_obj_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[host]", 6, http_request->host, http_request->host_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[port]", 6, http_request->port, http_request->port_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[H]", 3, http_request->H, http_request->H_len);
        incomplete_head_len = strlen(incomplete_head);      // 更新HTTP HEADER长度
        //printf("%s", incomplete_head);                      // 打印HTTP HEADER


        char *new_incomplete_data;
        new_incomplete_data = (char *)realloc(in->incomplete_data, incomplete_head_len + 1);        // 更新incomplete_data堆内存
        if (new_incomplete_data == NULL)    {
            perror("realloc");
            return NULL;
        }
        in->incomplete_data = new_incomplete_data;
        memset(in->incomplete_data, 0, incomplete_head_len + 1);                                    // 清空incomplete_data数据
        memmove(in->incomplete_data, incomplete_head, incomplete_head_len + 1);                     // 更新incomplete_data数据
        in->incomplete_data_len = strlen(in->incomplete_data);                                      // 更新incomplete_data长度
        free(incomplete_head);                                                                      // 释放incomplete_head内存
    }

    free_http_request(http_request);
    free(http_request);
    return in->incomplete_data;
}
