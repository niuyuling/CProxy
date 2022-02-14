#include "conf.h"
#include "http_request.h"

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

/* 字符串预处理，设置转义字符 */
static void string_pretreatment(char *str, int *len)
{
    char *lf, *p, *ori_strs[] = { "\\r", "\\n", "\\b", "\\v", "\\f", "\\t", "\\a", "\\b", "\\0" }, to_chrs[] = { '\r', '\n', '\b', '\v', '\f', '\t', '\a', '\b', '\0' };
    int i;

    while ((lf = strchr(str, '\n')) != NULL) {
        for (p = lf + 1; *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'; p++)
            *len -= 1;
        strcpy(lf, p);
        *len -= 1;
    }
    for (i = 0; i < sizeof(to_chrs); i++) {
        for (p = strstr(str, ori_strs[i]); p; p = strstr(p, ori_strs[i])) {
            //支持\\r
            *(p - 1) == '\\' ? (*p--) : (*p = to_chrs[i]);
            memmove(p + 1, p + 2, strlen(p + 2));
            (*len)--;
        }
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
    string_pretreatment(*val_begin, &val_len);
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
        if (strcasecmp(var, "uid") == 0)
        {
            p->uid = atoi(val_begin);
        }
        else if (strcasecmp(var, "process") == 0)
        {
            p->process = atoi(val_begin);
        }
        else if (strcasecmp(var, "timeout") == 0)
        {
            p->timeout = atoi(val_begin);
        }
        else if (strcasecmp(var, "encode") == 0)
        {
            p->sslencoding = atoi(val_begin);
        }
        else if (strcasecmp(var, "tcp_listen") == 0)
        {
            p->tcp_listen = atoi(val_begin);
        }
        else if (strcasecmp(var, "tcp6_listen") == 0)
        {
            p->tcp6_listen = atoi(val_begin);
        }
        else if (strcasecmp(var, "dns_listen") == 0)
        {
            p->dns_listen = atoi(val_begin);
        }
        else if (strcasecmp(var, "udp_listen") == 0)
        {
            p->udp_listen = atoi(val_begin);;
        }

        content = strchr(lineEnd + 1, '\n');
    }
}

static void parse_http_module(char *content, conf * p)
{
    char *var, *val_begin, *val_end, *lineEnd;
    tcp *http_node = NULL;
    char *p1 = NULL, *s = NULL, *t = NULL;
    char *p2 = NULL;

    while ((lineEnd = set_var_val_lineEnd(content, &var, &val_begin, &val_end)) != NULL) {
        if (strcasecmp(var, "http_ip") == 0)
        {
            p->http_ip_len = val_end - val_begin;
            if (copy_new_mem(val_begin, p->http_ip_len, &p->http_ip) != 0)
                return ;
        }
        else if (strcasecmp(var, "http_port") == 0)
        {
            p->http_port = atoi(val_begin);
        }
        else if (strcasecmp(var, "http_del") == 0)
        {
            p->http_del_len = val_end - val_begin;
            if (copy_new_mem(val_begin, p->http_del_len, &p->http_del) != 0)
                return ;
        }
        else if (strcasecmp(var, "http_first") == 0)
        {
            p->http_first_len = val_end - val_begin;
            if (copy_new_mem(val_begin, p->http_first_len, &p->http_first) != 0)
                return ;
        }
        else if (strcasecmp(var, "strrep") == 0)
        {
            http_node = (tcp *)malloc(sizeof(struct tcp));
            if (http_node == NULL)
                return ;
            memset(http_node, 0, sizeof(struct tcp));
            http_node->strrep = strdup(val_begin);
            http_node->strrep_len = val_end - val_begin;

            p1 = strstr(val_begin, "->");
            for (t = p1; *t != '"'; ++t) ;
            http_node->strrep_t = strdup(t + 1);
            p2 = strchr(t+1, '\0');
            http_node->strrep_t_len = p2 - (t + 1);

            for (s = p1 - 1; *s == ' '; s--) {
                if (s == val_begin)
                    return;
            }
            if (*s == '"')
                s--;

            http_node->strrep_s = strndup(val_begin, s - val_begin + 1);
            http_node->strrep_s_len = s - val_begin + 1;
            
            http_node->next = NULL;
            if (http_head_strrep == NULL) {
                http_head_strrep = http_node;
            } else {
                http_node->next = http_head_strrep;
                http_head_strrep = http_node;
                
                //http_node->next = http_head_strrep->next;
                //http_head_strrep->next = http_node;
            }
        } else if (strcasecmp(var, "regrep") == 0) {
            http_node = (tcp *) malloc(sizeof(struct tcp));
            if (http_node == NULL)
                return ;
            memset(http_node, 0, sizeof(struct tcp));
            http_node->regrep = strdup(val_begin);
            http_node->regrep_len = val_end - val_begin;
            
            p1 = strstr(val_begin, "->");
            for (t = p1; *t != '"'; ++t) ;
            http_node->regrep_t = strdup(t + 1);
            p2 = strchr(t+1, '\0');
            http_node->regrep_t_len = p2 - (t + 1);

            for (s = p1 - 1; *s == ' '; s--) {
                if (s == val_begin)
                    return ;
            }
            if (*s == '"')
                s--;

            http_node->regrep_s = strndup(val_begin, s - val_begin + 1);
            http_node->regrep_s_len = s - val_begin + 1;

            http_node->next = NULL;
            if (http_head_regrep == NULL) {
                http_head_regrep = http_node;
            } else {
                http_node->next = http_head_regrep;
                http_head_regrep = http_node;
                
                //http_node->next = http_head_regrep->next;
                //http_head_regrep->next = http_node;
            }
        }

        content = strchr(lineEnd + 1, '\n');
    }
}

static void parse_https_module(char *content, conf * p)
{
    char *var, *val_begin, *val_end, *lineEnd;
    tcp *https_node = NULL;
    char *p1 = NULL, *s = NULL, *t = NULL;
    char *p2 = NULL;

    while ((lineEnd = set_var_val_lineEnd(content, &var, &val_begin, &val_end)) != NULL) {
        if (strcasecmp(var, "https_ip") == 0)
        {
            p->https_ip_len = val_end - val_begin;
            if (copy_new_mem(val_begin, p->https_ip_len, &p->https_ip) != 0)
                return ;
        }
        else if (strcasecmp(var, "https_port") == 0)
        {
            p->https_port = atoi(val_begin);
        }
        else if (strcasecmp(var, "https_del") == 0)
        {
            p->https_del_len = val_end - val_begin;
            if (copy_new_mem(val_begin, p->https_del_len, &p->https_del) != 0)
                return ;
        }
        else if (strcasecmp(var, "https_first") == 0)
        {
            
            p->https_first_len = val_end - val_begin;
            if (copy_new_mem(val_begin, p->https_first_len, &p->https_first) != 0)
                return ;
            
        }
        else if (strcasecmp(var, "strrep") == 0)
        {
            // 链表操作,支持多个相同配置KEY
            https_node = (tcp *)malloc(sizeof(struct tcp));
            if (https_node == NULL)
                return ;
            memset(https_node, 0, sizeof(struct tcp));
            https_node->strrep = strdup(val_begin);
            https_node->strrep_len = val_end - val_begin;

            p1 = strstr(val_begin, "->");
            for (t = p1; *t != '"'; ++t) ;
            https_node->strrep_t = strdup(t + 1);
            p2 = strchr(t+1, '\0');
            https_node->strrep_t_len = p2 - (t + 1);

            for (s = p1 - 1; *s == ' '; s--) {
                if (s == val_begin)
                    return;
            }
            if (*s == '"')
                s--;

            https_node->strrep_s = strndup(val_begin, s - val_begin + 1);
            https_node->strrep_s_len = s - val_begin + 1;
            
            https_node->next = NULL;
            
            if (https_head_strrep == NULL) {
                https_head_strrep = https_node;
            } else {
                https_node->next = https_head_strrep;
                https_head_strrep = https_node;
                
                //https_node->next = https_head_strrep->next;
                //https_head_strrep->next = https_node;
            }
        } 
        else if (strcasecmp(var, "regrep") == 0)
        {
            https_node = (tcp *) malloc(sizeof(struct tcp));
            if (https_node == NULL)
                return ;
            memset(https_node, 0, sizeof(struct tcp));
            https_node->regrep = strdup(val_begin);
            https_node->regrep_len = val_end - val_begin;
            
            p1 = strstr(val_begin, "->");
            for (t = p1; *t != '"'; ++t) ;
            https_node->regrep_t = strdup(t + 1);
            p2 = strchr(t+1, '\0');
            https_node->regrep_t_len = p2 - (t + 1);

            for (s = p1 - 1; *s == ' '; s--) {
                if (s == val_begin)
                    return ;
            }
            if (*s == '"')
                s--;

            https_node->regrep_s = strndup(val_begin, s - val_begin + 1);
            https_node->regrep_s_len = s - val_begin + 1;

            https_node->next = NULL;
            if (https_head_regrep == NULL) {
                https_head_regrep = https_node;
            } else {
                https_node->next = https_head_regrep;
                https_head_regrep = https_node;
                
                //https_node->next = https_head_regrep->next;
                //https_head_regrep->next = https_node;
            }
        }

        content = strchr(lineEnd + 1, '\n');
    }
}

// 打印tcp链表
void print_tcp(tcp * p)
{
    tcp *temp = p;
    while (temp) {
        if (temp->strrep)
            printf("%s %d\n", temp->strrep, temp->strrep_len);
        if (temp->strrep_s)
            printf("%s %d\n", temp->strrep_s, temp->strrep_s_len);
        if (temp->strrep_t)
            printf("%s %d\n", temp->strrep_t, temp->strrep_t_len);

        if (temp->regrep)
            printf("%s %d\n", temp->regrep, temp->regrep_len);
        if (temp->regrep_s)
            printf("%s %d\n", temp->regrep_s, temp->regrep_s_len);
        if (temp->regrep_t)
            printf("%s %d\n", temp->regrep_t, temp->regrep_t_len);

        temp = temp->next;
    }
}


tcp *local_reverse(tcp *head)
{
    tcp *beg = NULL;
    tcp *end = NULL;
    if (head == NULL || head->next == NULL) {
        return head;
    }
    beg = head;
    end = head->next;
    while (end != NULL) {
        //将 end 从链表中摘除
        beg->next = end->next;
        //将 end 移动至链表头
        end->next = head;
        head = end;
        //调整 end 的指向，另其指向 beg 后的一个节点，为反转下一个节点做准备
        end = beg->next;
    }
    return head;
}

// Free tcp 链表
void free_tcp(tcp **conf_head)
{
    tcp *t;
    while(*conf_head != NULL)
    {
        t=*conf_head;
        *conf_head=t->next;
        
        if (t->strrep)
            free(t->strrep);
        if (t->strrep_s)
            free(t->strrep_s);
        if (t->strrep_t)
            free(t->strrep_t);
        
        if (t->regrep)
            free(t->regrep);
        if (t->regrep_s)
            free(t->regrep_s);
        if (t->regrep_t)
            free(t->regrep_t);
        if (t)
            free(t);
    }
}

static void parse_httpdns_module(char *content, conf * p)
{
    char *var, *val_begin, *val_end, *lineEnd;

    while ((lineEnd = set_var_val_lineEnd(content, &var, &val_begin, &val_end)) != NULL) {
        if (strcasecmp(var, "addr") == 0)
        {
            p->addr_len = val_end - val_begin;
            if (copy_new_mem(val_begin, p->addr_len, &p->addr) != 0)
                return ;
        }
        else if (strcasecmp(var, "http_req") == 0)
        {
            p->http_req_len = val_end - val_begin;
            if (copy_new_mem(val_begin, p->http_req_len, &p->http_req) != 0)
                return ;
        }
        else if (strcasecmp(var, "encode") == 0)
        {
            p->encode = atoi(val_begin);
        }
        
        content = strchr(lineEnd + 1, '\n');
    }
}

static void parse_httpudp_module(char *content, conf * p)
{
    char *var, *val_begin, *val_end, *lineEnd;

    while ((lineEnd = set_var_val_lineEnd(content, &var, &val_begin, &val_end)) != NULL) {
        if (strcasecmp(var, "addr") == 0)
        {
            p->httpudp_addr_len = val_end - val_begin;
            if (copy_new_mem(val_begin, p->httpudp_addr_len, &p->httpudp_addr) != 0)
                return ;
        }
        else if (strcasecmp(var, "http_req") == 0)
        {
            p->httpudp_http_req_len = val_end - val_begin;
            if (copy_new_mem(val_begin, p->httpudp_http_req_len, &p->httpudp_http_req) != 0)
                return ;
        }
        else if (strcasecmp(var, "encode") == 0)
        {
            p->httpudp_encode = atoi(val_begin);
        }
        
        content = strchr(lineEnd + 1, '\n');
    }
}

void free_conf(conf * p)
{
    // http module
    if (p->http_ip)
        free(p->http_ip);
    if (p->http_del)
        free(p->http_del);
    if (p->http_first)
        free(p->http_first);

    // https module
    if (p->https_ip)
        free(p->https_ip);
    if (p->https_del)
        free(p->https_del);
    if (p->https_first)
        free(p->https_first);

    // httpdns module
    if (p->addr)
        free(p->addr);
    if (p->http_req) {
        p->http_req_len = 0;
        free(p->http_req);
    }
    
    // httpudp module
    if(p->httpudp_addr)
        free(p->httpudp_addr);
    if (p->httpudp_http_req) {
        p->httpudp_http_req_len = 0;
        free(p->httpudp_http_req);
    }
    
    return;
}

void read_conf(char *filename, conf * configure)
{
    char *buff, *global_content, *http_content, *https_content, *httpdns_content, *httpudp_content;
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
    else
        parse_global_module(global_content, configure);
    free(global_content);

    if ((http_content = read_module(buff, "http")) == NULL)
        perror("read http module error");
    else
        parse_http_module(http_content, configure);
    free(http_content);

    if ((https_content = read_module(buff, "https")) == NULL)
        perror("read https module error");
    else
        parse_https_module(https_content, configure);
    free(https_content);

    if ((httpdns_content = read_module(buff, "httpdns")) == NULL)
        perror("read httpdns module error");
    else
        parse_httpdns_module(httpdns_content, configure);
    free(httpdns_content);
    
    if ((httpudp_content = read_module(buff, "httpudp")) == NULL)
        perror("read httpdns module error");
    else
        parse_httpudp_module(httpudp_content, configure);
    free(httpudp_content);
}
