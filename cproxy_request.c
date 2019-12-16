#include "cproxy_request.h"

// 字符串替换
char *replace(char *replace_memory, int *replace_memory_len, const char *src,
              const int src_len, const char *dest, const int dest_len)
{
    if (!replace_memory || !src || !dest)
        return replace_memory;

    char *p;
    int diff;

    if (src_len == dest_len) {
        for (p = memmem(replace_memory, *replace_memory_len, src, src_len); p;
             p =
             memmem(p, *replace_memory_len - (p - replace_memory), src,
                    src_len)) {
            memcpy(p, dest, dest_len);
            p += dest_len;
        }
    } else if (src_len < dest_len) {
        int before_len;
        char *before_end, *new_replace_memory;

        diff = dest_len - src_len;
        for (p = memmem(replace_memory, *replace_memory_len, src, src_len); p;
             p =
             memmem(p, *replace_memory_len - (p - replace_memory), src,
                    src_len)) {
            *replace_memory_len += diff;
            before_len = p - replace_memory;
            new_replace_memory =
                (char *)realloc(replace_memory, *replace_memory_len + 1);
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
        for (p = memmem(replace_memory, *replace_memory_len, src, src_len); p;
             p =
             memmem(p, *replace_memory_len - (p - replace_memory), src,
                    src_len)) {
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
static char *regrep(char *str, int *str_len, const char *src, char *dest,
                    int dest_len)
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
                real_dest =
                    replace(real_dest, &real_dest_len, child_num, 2,
                            p + pm[i].rm_so, pm[i].rm_eo - pm[i].rm_so);
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
                memmove(p + real_dest_len, p + match_len,
                        *str_len - (p + match_len - str));
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

// 删除单个字符
void del_chr(char *s, char ch)
{
    char *t = s;                // 目标指针先指向原串头
    while (*s != '\0')          // 遍历字符串s
    {
        if (*s != ch)           // 如果当前字符不是要删除的，则保存到目标串中
            *t++ = *s;
        s++;                    // 检查下一个字符
    }
    *t = '\0';                  // 置目标串结束符。
}

// strncpy()封装
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

uint8_t request_type(char *req)
{
    if (strncmp(req, "GET", 3) == 0 || strncmp(req, "POST", 4) == 0)
        return HTTP;
    else if (strncmp(req, "CONNECT", 7) == 0)
        return HTTP_CONNECT;
    else if (strncmp(req, "HEAD", 4) == 0 ||
             strncmp(req, "PUT", 3) == 0 ||
             strncmp(req, "OPTIONS", 7) == 0 ||
             strncmp(req, "MOVE", 4) == 0 ||
             strncmp(req, "COPY", 4) == 0 ||
             strncmp(req, "TRACE", 5) == 0 ||
             strncmp(req, "DELETE", 6) == 0 ||
             strncmp(req, "LINK", 4) == 0 ||
             strncmp(req, "UNLINK", 6) == 0 ||
             strncmp(req, "PATCH", 5) == 0 || strncmp(req, "WRAPPED", 7) == 0)
        return HTTP_OTHERS;
    else
        return OTHER;
}

int extract_host(char *header)
{
    char *_p = strstr(header, "CONNECT"); // 在 CONNECT 方法中解析 隧道主机名称及端口号
    if (_p) {
        char *_p1 = strchr(_p, ' ');
        char *_p2 = strchr(_p1 + 1, ':');
        char *_p3 = strchr(_p1 + 1, ' ');

        if (_p2) {
            char s_port[10];
            bzero(s_port, 10);
            strncpy(remote_host, _p1 + 1, (int)(_p2 - _p1) - 1);
            strncpy(s_port, _p2 + 1, (int)(_p3 - _p2) - 1);
            remote_port = atoi(s_port);

        } else {
            strncpy(remote_host, _p1 + 1, (int)(_p3 - _p1) - 1);
            remote_port = 80;
        }
        return 0;
    }

    char *p = strstr(header, "Host:");
    if (!p) {
        return -1;
    }
    char *p1 = strchr(p, '\n');
    if (!p1) {
        return -1;
    }

    char *p2 = strchr(p + 5, ':'); // 5是指'Host:'的长度
    if (p2 && p2 < p1) {
        int p_len = (int)(p1 - p2 - 1);
        char s_port[p_len];
        strncpy(s_port, p2 + 1, p_len);
        s_port[p_len] = '\0';
        remote_port = atoi(s_port);

        int h_len = (int)(p2 - p - 5 - 1);
        strncpy(remote_host, p + 5 + 1, h_len); // Host:
        remote_host[h_len] = '\0';
    } else {
        int h_len = (int)(p1 - p - 5 - 1 - 1);
        strncpy(remote_host, p + 5 + 1, h_len);
        remote_host[h_len] = '\0';
        remote_port = 80;
    }

    return 0;
}

// 转发头字符串到destination_sock
void forward_header(int destination_sock)
{
    rewrite_header();
    int len = strlen(header_buffer);
    send_data(destination_sock, header_buffer, len);
    return;
}

// 代理中的完整URL转发前需改成 path 的形式
void rewrite_header()
{
    char *p = strstr(header_buffer, "http://");
    char *p0 = strchr(p, '\0');
    char *p5 = strstr(header_buffer, "HTTP/"); // "HTTP/" 是协议标识 如 "HTTP/1.1"
    int len = strlen(header_buffer);
    if (p) {
        char *p1 = strchr(p + 7, '/');
        if (p1 && (p5 > p1)) {
            // 转换url到 path
            memcpy(p, p1, (int)(p0 - p1));
            int l = len - (p1 - p);
            header_buffer[l] = '\0';
        } else {
            char *p2 = strchr(p, ' '); // GET http://3g.sina.com.cn HTTP/1.1
            memcpy(p + 1, p2, (int)(p0 - p2));
            *p = '/';           // url 没有路径使用根
            int l = len - (p2 - p) + 1;
            header_buffer[l] = '\0';
        }
    }
    return;
}

// 判断数字有几位
int numbin(int n)
{
    int sum = 0;
    while (n) {
        sum++;
        n /= 10;
    }
    return sum;
}

// 删除字符串header_buffer中第一位到character处,并拼接string,character为空返回原字符串.(string替换第一个字符到character处)
char *splice_head(char *header_buffer, const char *character, char *string)
{
    int len_first = strlen(string);
    char *p1 = strstr(header_buffer, character);
    if (p1 == NULL) {
        return header_buffer;
    }
    p1 = p1 + 1;
    char new_string[len_first + strlen(p1) + 1];
    strcpy(new_string, string); // 拼接
    return strcat(new_string, p1);
}

// 删除字符串header_buffer中character到string处,character为空返回原字符串.
char *delete_header(char *header_buffer, const char *character, int string)
{
    int len_header_buffer = strlen(header_buffer);
    char *p1 = strstr(header_buffer, character);
    if (p1 == NULL) {
        return header_buffer;
    }
    char *p2 = strchr(p1, string);
    int l = len_header_buffer - strlen(p1);
    header_buffer[l] = '\0';
    return strcat(header_buffer, p2 + 1);
}

char *splice_host_port(char *tmp, char *host, char *port)
{
    //memset(tmp, 0, strlen(tmp));
    bzero(tmp, strlen(tmp));
    strcat(tmp, host);
    strcat(tmp, ":");
    return strcat(tmp, port);
}

int replacement_http_head(char *header_buffer, char *remote_host,
                          int *remote_port, int *SIGN, conf * p)
{
    char *http_firsts = (char *)malloc(strlen(p->http_first) + 1);
    if (http_firsts) {
        strcpy(http_firsts, p->http_first); // 拷贝http_first
    } else {
        free(http_firsts);
        return 0;
    }
    char *https_firsts = (char *)malloc(strlen(p->https_first) + 1);
    if (https_firsts) {
        strcpy(https_firsts, p->https_first); // 拷贝https_first
    } else {
        free(https_firsts);
        return 0;
    }

    char *header_buffer_backup = (char *)malloc(strlen(header_buffer) + 1); // 拷贝原字符串
    if (header_buffer_backup) {
        strcpy(header_buffer_backup, header_buffer);
    } else {
        free(header_buffer_backup);
        return 0;
    }

    char *new_http_del = malloc(strlen(p->http_del) + 1); // 拷贝http_del
    if (new_http_del) {
        strcpy(new_http_del, p->http_del);
    } else {
        free(new_http_del);
        return 0;
    }

    char *new_https_del = malloc(strlen(p->https_del) + 1); // 拷贝https_del
    if (new_https_del) {
        strcpy(new_https_del, p->https_del);
    } else {
        free(new_https_del);
        return 0;
    }

    if (*SIGN == HTTP) {
        char *result = NULL;
        result = strtok(new_http_del, ",");
        while (result != NULL) {
            delete_header(header_buffer_backup, result, '\n');
            result = strtok(NULL, ",");
        }

        char *p2 = strstr(header_buffer_backup, "\n");
        p2 = p2 + 1;
        int len_http_head = strlen(header_buffer_backup) - strlen(p2);
        char *HTTP_HEAD = (char *)malloc(len_http_head + 1); // http头第一行
        if (HTTP_HEAD) {
            strncpy_(HTTP_HEAD, header_buffer_backup, len_http_head);
        } else {
            free(HTTP_HEAD);
            return 0;
        }

        // M
        char *p3 = strstr(HTTP_HEAD, " ");
        int l = strlen(HTTP_HEAD) - strlen(p3);
        char *M = malloc(l + 1);
        if (M) {
            strncpy_(M, HTTP_HEAD, l);
            //printf("%s", M);
        } else {
            free(M);
            return 0;
        }

        // U
        p3 = p3 + 1;
        char *p4 = strstr(p3, " ");
        l = strlen(p3) - strlen(p4);
        char *U = (char *)malloc(l + 1);
        if (U) {
            strncpy_(U, p3, l);
            //printf("%s", U);
        } else {
            free(U);
            return 0;
        }

        // V
        p4 = p4 + 1;
        del_chr(p4, '\r');
        del_chr(p4, '\n');
        l = strlen(p4);
        char *V = (char *)malloc(l);
        if (V) {
            strcpy(V, p4);
            //printf("%s", V);
        } else {
            free(V);
            return 0;
        }

        char *new_header_buffer =
            (char *)
            malloc(strlen(splice_head(header_buffer_backup, "\n", http_firsts))
                   + 1);
        if (new_header_buffer) {
            strcpy(new_header_buffer,
                   splice_head(header_buffer_backup, "\n", http_firsts));
        } else {
            free(new_header_buffer);
            return 0;
        }

        int len = strlen(new_header_buffer);
        int len_m = strlen(M);
        int len_u = strlen(U);
        int len_v = strlen(V);
        int len_remote_host = strlen(remote_host);

        new_header_buffer =
            replace(new_header_buffer, &len, "[M]", 3, M, len_m);
        new_header_buffer =
            replace(new_header_buffer, &len, "[U]", 3, U, len_u);
        new_header_buffer =
            replace(new_header_buffer, &len, "[V]", 3, V, len_v);
        new_header_buffer =
            replace(new_header_buffer, &len, "[host]", 6, remote_host,
                    len_remote_host);

        char port_copy[(numbin(*remote_port) + 2)];
        sprintf(port_copy, "%d", *remote_port);
        int len_remote_port = strlen(port_copy);
        new_header_buffer =
            replace(new_header_buffer, &len, "[port]", 6, port_copy,
                    len_remote_port);

        char H[(len_remote_port + len_remote_host) + 1];
        splice_host_port(H, remote_host, port_copy);
        int len_h = strlen(H);
        new_header_buffer =
            replace(new_header_buffer, &len, "[H]", 3, H, len_h);

        new_header_buffer = replace(new_header_buffer, &len, "\\r", 2, "\r", 1);
        new_header_buffer = replace(new_header_buffer, &len, "\\n", 2, "\n", 1);
        new_header_buffer = replace(new_header_buffer, &len, "\\b", 2, "\b", 1);
        new_header_buffer = replace(new_header_buffer, &len, "\\v", 2, "\v", 1);
        new_header_buffer = replace(new_header_buffer, &len, "\\f", 2, "\f", 1);
        new_header_buffer = replace(new_header_buffer, &len, "\\a", 2, "\a", 1);
        new_header_buffer = replace(new_header_buffer, &len, "\\t", 2, "\t", 1);
        new_header_buffer = replace(new_header_buffer, &len, "\\r", 2, "\r", 1);
        new_header_buffer = replace(new_header_buffer, &len, "\\n", 2, "\n", 1);
        if (p->http_strrep) {
            int regrep_aim_len = strlen(p->http_strrep_aim);
            int regrep_obj_len = strlen(p->http_strrep_obj);
            new_header_buffer =
                replace(new_header_buffer, &len, p->http_strrep_aim,
                        regrep_aim_len, p->http_strrep_obj, regrep_obj_len);
        }
        if (p->http_regrep) {
            len = strlen(new_header_buffer) + 1;
            new_header_buffer =
                regrep(new_header_buffer, &len, p->http_regrep_aim,
                       p->http_regrep_obj, strlen(p->http_regrep_obj));
        }
        //stpcpy(p->http_ip, remote_host);
        //p->http_port = *remote_port;
        memset(header_buffer, 0, strlen(header_buffer));
        strcpy(header_buffer, new_header_buffer);
        len_header_buffer = strlen(header_buffer);

        free(HTTP_HEAD);
        free(M);
        free(U);
        free(V);
        free(new_header_buffer);

    } else if (*SIGN == HTTP_CONNECT) {
        char *result = NULL;
        result = strtok(new_https_del, ",");
        while (result != NULL) {
            delete_header(header_buffer_backup, result, '\n');
            result = strtok(NULL, ",");
        }

        char *p2 = strstr(header_buffer_backup, "\n");
        p2 = p2 + 1;
        int len_https_head = strlen(header_buffer_backup) - strlen(p2);
        char *HTTPS_HEAD = (char *)malloc(len_https_head + 1); // https头第一行
        if (HTTPS_HEAD) {
            strncpy_(HTTPS_HEAD, header_buffer_backup, len_https_head);
        } else {
            free(HTTPS_HEAD);
            return 0;
        }

        // M
        char *p3 = strstr(HTTPS_HEAD, " ");
        int l = strlen(HTTPS_HEAD) - strlen(p3);
        char *M = malloc(l + 1);
        if (M) {
            strncpy_(M, HTTPS_HEAD, l);
        } else {
            free(M);
            return 0;
        }

        // U
        p3 = p3 + 1;
        char *p4 = strstr(p3, " ");
        l = strlen(p3) - strlen(p4);
        char *U = (char *)malloc(l + 1);
        if (U) {
            strncpy_(U, p3, l);
            //printf("%s", U);
        } else {
            free(U);
            return 0;
        }

        // V
        p4 = p4 + 1;
        l = strlen(p4);
        char *V = (char *)malloc(l);
        if (V) {
            strncpy_(V, p4, 8);
            //printf("%s", V);
        } else {
            free(V);
            return 0;
        }

        char *new_header_buffer =
            (char *)
            malloc(strlen(splice_head(header_buffer_backup, "\n", https_firsts))
                   + 1);
        if (new_header_buffer) {
            strcpy(new_header_buffer,
                   splice_head(header_buffer_backup, "\n", https_firsts));
        } else {
            free(new_header_buffer);
            return 0;
        }

        int len = strlen(new_header_buffer);
        int len_m = strlen(M);
        int len_u = strlen(U);
        int len_v = strlen(V);
        int len_remote_host = strlen(remote_host);

        new_header_buffer =
            replace(new_header_buffer, &len, "[M]", 3, M, len_m);
        new_header_buffer =
            replace(new_header_buffer, &len, "[U]", 3, U, len_u);
        new_header_buffer =
            replace(new_header_buffer, &len, "[V]", 3, V, len_v);
        new_header_buffer =
            replace(new_header_buffer, &len, "[host]", 6, remote_host,
                    len_remote_host);

        char port_copy[(numbin(*remote_port) + 2)];
        sprintf(port_copy, "%d", *remote_port);
        int len_remote_port = strlen(port_copy);
        new_header_buffer =
            replace(new_header_buffer, &len, "[port]", 6, port_copy,
                    len_remote_port);
        char H[(len_remote_port + len_remote_host) + 1];
        splice_host_port(H, remote_host, port_copy);
        int len_h = strlen(H);
        new_header_buffer =
            replace(new_header_buffer, &len, "[H]", 3, H, len_h);


        new_header_buffer = replace(new_header_buffer, &len, "\\r", 2, "\r", 1);
        new_header_buffer = replace(new_header_buffer, &len, "\\n", 2, "\n", 1);
        new_header_buffer = replace(new_header_buffer, &len, "\\b", 2, "\b", 1);
        new_header_buffer = replace(new_header_buffer, &len, "\\v", 2, "\v", 1);
        new_header_buffer = replace(new_header_buffer, &len, "\\f", 2, "\f", 1);
        new_header_buffer = replace(new_header_buffer, &len, "\\a", 2, "\a", 1);
        new_header_buffer = replace(new_header_buffer, &len, "\\t", 2, "\t", 1);
        new_header_buffer = replace(new_header_buffer, &len, "\\r", 2, "\r", 1);
        new_header_buffer = replace(new_header_buffer, &len, "\\n", 2, "\n", 1);

        if (p->https_strrep) {
            int regrep_aim_len = strlen(p->https_strrep_aim);
            int regrep_obj_len = strlen(p->https_strrep_obj);
            new_header_buffer =
                replace(new_header_buffer, &len, p->https_strrep_aim,
                        regrep_aim_len, p->https_strrep_obj, regrep_obj_len);
        }
        if (p->https_regrep) {
            len = strlen(new_header_buffer) + 1;
            new_header_buffer =
                regrep(new_header_buffer, &len, p->https_regrep_aim,
                       p->https_regrep_obj, strlen(p->https_regrep_obj));

        }
            
        //stpcpy(p->https_ip, remote_host); // 走真实IP非代理
        //p->https_port = *remote_port;
        memset(header_buffer, 0, strlen(header_buffer));
        memcpy(header_buffer, new_header_buffer, strlen(new_header_buffer));
        len_header_buffer = strlen(header_buffer);

        free(HTTPS_HEAD);
        free(M);
        free(U);
        free(V);
        free(new_header_buffer);

    }

    free(http_firsts);
    free(https_firsts);
    free(new_http_del);
    free(new_https_del);
    free(header_buffer_backup);
    return 1;
}
