#include "cproxy_request.h"

void *memmem(const void *haystack, size_t haystacklen, const void *needle,
             size_t needlelen);

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

// 删除单个字符
void del_chr(char *s, char ch)
{
    char *t = s;                //目标指针先指向原串头
    while (*s != '\0')          //遍历字符串s
    {
        if (*s != ch)           //如果当前字符不是要删除的，则保存到目标串中
            *t++ = *s;
        s++;                    //检查下一个字符
    }
    *t = '\0';                  //置目标串结束符。
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

// 删除字符串header_buffer中第一位到ch处,并拼接str,ch必须存在.
char *splice_head(char *header_buffer, const char *ch, char *str)
{
    int len_http_first = strlen(str);
    char *p1 = strstr(header_buffer, ch);
    p1 = p1 + 1;

    char *s = (char *)malloc(len_http_first + strlen(p1) + 1);
    strcpy(s, str);             // 拼接
    return strcat(s, p1);
}

// 删除字符串header_buffer中ch到str处,ch为空返回原字符串.
char *delete_header(char *header_buffer, const char *ch, int str)
{
    int len_header_buffer = strlen(header_buffer);
    char *p1 = strstr(header_buffer, ch);
    if (p1 == NULL) {
        return header_buffer;
    }
    char *p2 = strchr(p1, str);
    int l = len_header_buffer - strlen(p1);
    header_buffer[l] = '\0';
    return strcat(header_buffer, p2 + 1);
}

int replacement_http_head(char *header_buffer, char *remote_host,
                          int *remote_port, int *HTTPS, conf *p)
{
    char *http_firsts = (char *)malloc(strlen(p->http_first) + 1);
    strcpy(http_firsts, p->http_first); // 拷贝http_first
    char *https_firsts = (char *)malloc(strlen(p->https_first) + 1);
    strcpy(https_firsts, p->https_first); // 拷贝https_first

    char *header_buffers = (char *)malloc(strlen(header_buffer) + 1); // 拷贝原字符串
    strcpy(header_buffers, header_buffer);

    char *new_http_del = malloc(strlen(p->http_del) + 1); // 拷贝http_del
    strcpy(new_http_del, p->http_del);

    char *new_https_del = malloc(strlen(p->https_del) + 1); // // 拷贝https_del
    strcpy(new_https_del, p->https_del);

    char *con = strstr(header_buffers, "CONNECT"); // 判断是否是http 隧道请求
    if (con == NULL) {
        char *result = NULL;
        result = strtok(new_http_del, ",");
        while (result != NULL) {
            delete_header(header_buffers, result, '\n');
            result = strtok(NULL, ",");
        }
        //delete_header(header_buffers, "Host", '\n'); // 删除HOST,改变原字符串
        char *new_header_buffer = (char *)
            malloc(strlen(splice_head(header_buffers, "\n", http_firsts)) + 1);
        strcpy(new_header_buffer,
               splice_head(header_buffers, "\n", http_firsts));

        char *p2 = strstr(header_buffers, "\n");
        p2 = p2 + 1;
        int len_http_head = strlen(header_buffers) - strlen(p2);
        char *HTTP_HEAD = (char *)malloc(len_http_head + 1); //http头第一行
        strncpy_(HTTP_HEAD, header_buffers, len_http_head);

        // M
        char *p3 = strstr(HTTP_HEAD, " ");
        int l = strlen(HTTP_HEAD) - strlen(p3);
        char *M = malloc(l + 1);
        strncpy_(M, HTTP_HEAD, l);
        //printf("%s", M);

        // U
        p3 = p3 + 1;
        char *p4 = strstr(p3, " ");
        l = strlen(p3) - strlen(p4);
        char *U = (char *)malloc(l + 1);
        strncpy_(U, p3, l);
        //printf("%s", U);

        // V
        p4 = p4 + 1;
        del_chr(p4, '\r');
        del_chr(p4, '\n');
        l = strlen(p4);
        char *V = (char *)malloc(l);
        strcpy(V, p4);
        //printf("%s", V);

        int len = strlen(new_header_buffer);
        int len_m = strlen(M);
        new_header_buffer =
            replace(new_header_buffer, &len, "[M]", 3, M, len_m);
        len = strlen(new_header_buffer);
        int len_u = strlen(U);
        new_header_buffer =
            replace(new_header_buffer, &len, "[U]", 3, U, len_u);
        len = strlen(new_header_buffer);
        int len_v = strlen(V);
        new_header_buffer =
            replace(new_header_buffer, &len, "[V]", 3, V, len_v);
        len = strlen(new_header_buffer);
        int len_remote_host = strlen(remote_host);
        new_header_buffer =
            replace(new_header_buffer, &len, "[host]", 6, remote_host,
                    len_remote_host);
        len = strlen(new_header_buffer);
        new_header_buffer = replace(new_header_buffer, &len, "\\r", 2, "\r", 1);
        len = strlen(new_header_buffer);
        new_header_buffer = replace(new_header_buffer, &len, "\\n", 2, "\n", 1);

        stpcpy(remote_host, p->http_ip);
        *remote_port = p->http_port;
        memset(header_buffer, 0, strlen(header_buffer));
        strcpy(header_buffer, new_header_buffer);
        free(HTTP_HEAD);
        free(M);
        free(U);
        free(V);
        free(new_header_buffer);
        *HTTPS = 0;
    } else {
        char *result = NULL;
        result = strtok(new_https_del, ",");
        while (result != NULL) {
            delete_header(header_buffers, result, '\n');
            result = strtok(NULL, ",");
        }
        //delete_header(header_buffers, "Host", '\n'); // 删除HOST,改变原字符串
        //delete_header(header_buffers, "x-online-host", '\n'); // 删除HOST,改变原字符串
        char *new_header_buffer = (char *)malloc(strlen(splice_head(header_buffers, "\n", https_firsts)) + 1);
        strcpy(new_header_buffer, splice_head(header_buffers, "\n", https_firsts));

        char *p2 = strstr(header_buffers, "\n");
        p2 = p2 + 1;
        int len_https_head = strlen(header_buffers) - strlen(p2);
        char *HTTPS_HEAD = (char *)malloc(len_https_head + 1); //http头第一行
        strncpy_(HTTPS_HEAD, header_buffers, len_https_head);

        // M
        char *p3 = strstr(HTTPS_HEAD, " ");
        int l = strlen(HTTPS_HEAD) - strlen(p3);
        char *M = malloc(l + 1);
        strncpy_(M, HTTPS_HEAD, l);
        //printf("%s", M);

        // U
        p3 = p3 + 1;
        char *p4 = strstr(p3, " ");
        l = strlen(p3) - strlen(p4);
        char *U = (char *)malloc(l + 1);
        strncpy_(U, p3, l);
        //printf("%s", U);

        // V
        p4 = p4 + 1;
        del_chr(p4, '\r');
        del_chr(p4, '\n');
        l = strlen(p4);
        char *V = (char *)malloc(l);
        strcpy(V, p4);
        //printf("%s", V);

        int len = strlen(new_header_buffer);
        int len_m = strlen(M);
        new_header_buffer =
            replace(new_header_buffer, &len, "[M]", 3, M, len_m);
        len = strlen(new_header_buffer);
        int len_u = strlen(U);
        new_header_buffer =
            replace(new_header_buffer, &len, "[U]", 3, U, len_u);
        len = strlen(new_header_buffer);
        int len_v = strlen(V);
        new_header_buffer =
            replace(new_header_buffer, &len, "[V]", 3, V, len_v);
        len = strlen(new_header_buffer);
        int len_remote_host = strlen(remote_host);
        new_header_buffer =
            replace(new_header_buffer, &len, "[host]", 6, remote_host,
                    len_remote_host);
        len = strlen(new_header_buffer);
        new_header_buffer = replace(new_header_buffer, &len, "\\r", 2, "\r", 1);
        len = strlen(new_header_buffer);
        new_header_buffer = replace(new_header_buffer, &len, "\\n", 2, "\n", 1);

        //stpcpy(remote_host, p->https_ip);
        //*remote_port = p->https_port;
        memset(header_buffer, 0, strlen(header_buffer));
        strcpy(header_buffer, new_header_buffer);

        free(HTTPS_HEAD);
        free(M);
        free(U);
        free(V);
        free(new_header_buffer);
        *HTTPS = 1;
    }
    free(http_firsts);
    free(https_firsts);
    free(new_http_del);
    free(new_https_del);
    free(header_buffers);
    return *HTTPS;
}
