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
    int len_first = strlen(string);
    char *p1 = strstr(head, character);
    if (p1 == NULL) {
        return head;
    }
    p1 = p1 + 1;
    char new_string[len_first + strlen(p1) + 1];
    strcpy(new_string, string);
    return strcat(new_string, p1);
}

// 删除字符串 head 中 character 到 string 处, character 为空返回原字符串.
char *delete_head(char *head, const char *character, int string)
{
    int head_len = strlen(head);
    char *p1 = strstr(head, character);
    if (p1 == NULL) {
        return head;
    }
    char *p2 = strchr(p1, string);
    if (p2 == NULL) {
        return head;
    }

    char tmp[head_len];
    strncpy_(tmp, head, head_len - strlen(p1) - 1);
    strcat(tmp, p2);
    return strcpy(head, tmp);
}

int extract_host(char *header, char *host, char *port)
{
    memset(host, 0, strlen(host));
    memset(port, 0, strlen(port));
    char *_p = strstr(header, "CONNECT"); // 在 CONNECT 方法中解析 隧道主机名称及端口号
    if (_p) {
        char *_p1 = strchr(_p, ' ');
        char *_p2 = strchr(_p1 + 1, ':');
        char *_p3 = strchr(_p1 + 1, ' ');

        if (_p2) {
            char s_port[10];
            bzero(s_port, 10);
            strncpy_(host, _p1 + 1, (int)(_p2 - _p1) - 1);
            strncpy_(s_port, _p2 + 1, (int)(_p3 - _p2) - 1);
            strcpy(port, s_port);

        } else {
            strncpy(host, _p1 + 1, (int)(_p3 - _p1) - 1);
            strcpy(port, "80");
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
        strcpy(port, s_port);

        int h_len = (int)(p2 - p - 5 - 1);
        strncpy(host, p + 5 + 1, h_len); // Host:
        host[h_len] = '\0';
    } else {
        int h_len = (int)(p1 - p - 5 - 1 - 1);
        strncpy_(host, p + 5 + 1, h_len);
        host[h_len] = '\0';
        strcpy(port, "80");
    }
    return 0;
}

char *get_path(char *url, char *path)
{
    if (url) {
        char *p0 = strstr(url + 7, "/");
        if (p0)
            return strncpy_(path, p0, strlen(p0));
        else
            return NULL;
    }
    return NULL;
}

void free_http_request(struct http_request *http_request) {
    if (http_request->M)
        free(http_request->M);
    if (http_request->U)
        free(http_request->U);
    if (http_request->V)
        free(http_request->V);
}

void parse_request_head(char *http_request_line, struct http_request *http_request) {
    char *p;
    char *head;
    size_t head_len;
    char *m, *u;
    
    p = strstr(http_request_line, "\r\n");       // 查找"\r\n"
    if(p == NULL) {
        return ;
    }
    
    head_len = strlen(http_request_line) - strlen(p);
    head = (char *)malloc(sizeof(char) * head_len*2);
    if (head == NULL)
        free(head);
    memset(head, 0, head_len*2);
    memcpy(head, http_request_line, head_len);
    //printf("HEAD: %s\n", head);
    
    http_request->M = (char *)malloc(sizeof(char) * head_len);
    http_request->U = (char *)malloc(sizeof(char) * head_len);
    http_request->V = (char *)malloc(10);
    if (http_request->M == NULL) {
        free(http_request->M);
        perror("malloc");
    }
    if (http_request->U == NULL) {
        free(http_request->M);
        perror("malloc");
    }
    if (http_request->V == NULL) {
        free(http_request->M);
        perror("malloc");
    }

    memset(http_request->M, 0, head_len);
    memset(http_request->U, 0, head_len);
    memset(http_request->V, 0, 10);

    m = strstr(head, " ");
    http_request->M_len = strlen(head) - strlen(m);
    memset(http_request->M, 0, head_len);
    memcpy(http_request->M, head, http_request->M_len);
   
    
    u = strstr(m+1, " ");
    http_request->U_len = strlen(m+1) - strlen(u);
    memset(http_request->U, 0, head_len);
    memcpy(http_request->U, m+1, http_request->U_len);
    
    memset(http_request->V, 0, 8);
    memcpy(http_request->V, u+1, 8);
    http_request->V_len = 8;

    free(head);
    printf("%s\n", http_request->M);
    printf("%s\n", http_request->U);
    printf("%s\n", http_request->V);
    printf("%d\n", http_request->M_len);
    printf("%d\n", http_request->U_len);
    printf("%d\n", http_request->V_len);

}

char *request_head(conn * in, conf * configure)
{
    struct http_request *http_request;
    http_request = (struct http_request *)malloc(sizeof(struct http_request));
    parse_request_head(in->header_buffer, http_request);
    
    //printfconf(configure);    // 打印配置
    if (strncmp(http_request->M, "CONNECT", 7) == 0) {
        char host[http_request->U_len], port[http_request->U_len], H[http_request->U_len * 2];
        char *incomplete_head;
        int incomplete_head_len;
        char https_del_copy[configure->https_del_len * 2];
        char *result = NULL;

        extract_host(in->header_buffer, host, port);

        if (configure->https_port > 0)
            remote_port = configure->https_port;
        if (configure->https_ip != NULL)
            strcpy(remote_host, configure->https_ip);
        incomplete_head = (char *)malloc(sizeof(char) * (1024 + BUFFER_SIZE));
        if (incomplete_head == NULL) {
            free(incomplete_head);
            perror("malloc");
        }
        memset(incomplete_head, 0, sizeof(char) * (1024 + BUFFER_SIZE));
        memcpy(incomplete_head, in->header_buffer, strlen(in->header_buffer));
        memcpy(https_del_copy, configure->https_del, configure->https_del_len);
        
        result = strtok(https_del_copy, ",");
        while (result != NULL) {
            delete_head(incomplete_head, result, '\n');
            result = strtok(NULL, ",");
        }
        strcpy(incomplete_head, splice_head(incomplete_head, "\n", configure->https_first));
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
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[host]", 6, host, (int)strlen(host));
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[port]", 6, port, (int)strlen(port));
        memset(H, 0, strlen(H));
        strcpy(H, host);
        strcat(H, ":");
        strcat(H, port);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[H]", 3, H, (int)strlen(H));
        if (configure->https_strrep) {
            incomplete_head = replace(incomplete_head, &incomplete_head_len, configure->https_strrep_aim, configure->https_strrep_aim_len, configure->https_strrep_obj, configure->https_strrep_obj_len);
        }
        if (configure->https_regrep) {
            incomplete_head = regrep(incomplete_head, &incomplete_head_len, configure->https_regrep_aim, configure->https_regrep_obj, configure->https_regrep_obj_len);
        }
        printf("%s", incomplete_head);    // 打印HTTP HEADER

        memset(in->header_buffer, 0, strlen(in->header_buffer));
        strcpy(in->header_buffer, incomplete_head);
        in->header_buffer_len = strlen(in->header_buffer);
        free(incomplete_head);

    } else {
        char host[http_request->U_len], port[http_request->U_len], H[http_request->U_len * 2];
        char url[http_request->U_len], uri[http_request->U_len];
        int uri_len;
        char *incomplete_head;
        int incomplete_head_len;
        char https_del_copy[configure->http_del_len];
        char *result = NULL;

        extract_host(in->header_buffer, host, port);
        strcpy(url, http_request->U);
        get_path(url, uri);
        uri_len = strlen(uri);
        
        if (configure->http_port > 0)
            remote_port = configure->http_port;
        if (configure->https_ip != NULL)
            strcpy(remote_host, configure->http_ip);
        incomplete_head = (char *)malloc(sizeof(char) * (1024 + BUFFER_SIZE));
        if (incomplete_head == NULL) {
            free(incomplete_head);
            perror("malloc");
        }
        memset(incomplete_head, 0, sizeof(char) * (1024 + BUFFER_SIZE));
        strcpy(incomplete_head, in->header_buffer);
        strcpy(https_del_copy, configure->http_del);
        
        result = strtok(https_del_copy, ",");
        while (result != NULL) {
            delete_head(incomplete_head, result, '\n');
            result = strtok(NULL, ",");
        }
        strcpy(incomplete_head, splice_head(incomplete_head, "\n", configure->http_first));
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
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[uri]", 5, uri, uri_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[V]", 3, http_request->V, http_request->V_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[version]", 9, http_request->V, http_request->V_len);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[host]", 6, host, (int)strlen(host));
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[port]", 6, port, (int)strlen(port));
        memset(H, 0, strlen(H));
        strcpy(H, host);
        strcat(H, ":");
        strcat(H, port);
        incomplete_head = replace(incomplete_head, &incomplete_head_len, "[H]", 3, H, (int)strlen(H));
        if (configure->http_strrep) {
            incomplete_head = replace(incomplete_head, &incomplete_head_len, configure->http_strrep_aim, configure->http_strrep_aim_len, configure->http_strrep_obj, configure->http_strrep_obj_len);
        }
        if (configure->http_regrep) {
            incomplete_head = regrep(incomplete_head, &incomplete_head_len, configure->http_regrep_aim, configure->http_regrep_obj, configure->http_regrep_obj_len);
        }
        printf("%s", incomplete_head);
        
        memset(in->header_buffer, 0, strlen(in->header_buffer));
        strcpy(in->header_buffer, incomplete_head);
        in->header_buffer_len = strlen(in->header_buffer);
        free(incomplete_head);
    }
    free_http_request(http_request);
    free(http_request);
    return in->header_buffer;
}
