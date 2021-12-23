#include <sys/wait.h>
#include <sys/stat.h>
#include "httpdns.h"
#include "http_request.h"

#define DNS_MAX_CONCURRENT 512
#define DNS_REQUEST_MAX_SIZE 512+2 //2是TCPDNS头部用于指定dns的长度
#define HTTP_RSP_SIZE 2048
/*
    缓存结构: (dns查询请求长度-2)(2字节) + dns原查询请求(删除2字节的dnsID) + dns回应长度(2字节) + dns回应
*/
struct dns_cache {
    char *dns_cache;
    struct dns_cache *next;
};
typedef struct dns_connection {
    char dns_req[DNS_REQUEST_MAX_SIZE + 1];
    struct sockaddr_in src_addr;
    char *out_request;
    char *host;
    int out_request_len, fd, timer;
    /*
       sent_CONNECT_len:
       在使用SSL代理的情况下，与httpDNS的CONNECT请求长度对比
       小于表示需要发送CONNECT请求，并且还没有发送完成
       等于表示已经完成CONNECT连接
       大于表示已发送完成CONNECT连接，但是没有读取CONNECT的回应包
     */
    int sent_CONNECT_len;
    uint16_t dns_req_len;
    char query_type;
    unsigned host_len:7;
} dns_t;

static dns_t dns_list[DNS_MAX_CONCURRENT];
static struct epoll_event dns_evs[DNS_MAX_CONCURRENT + 1], dns_ev;
struct httpdns httpdns;
static int dns_efd;
/* 缓存变量 */
FILE *cfp = NULL;
static struct dns_cache *cache = NULL;
static int cache_using;
//子进程先写入缓存，再到父进程写入，否则可能导致缓存文件错乱
pid_t child_pid = 0;

/* 读取缓存文件 */
int8_t read_cache_file()
{
    char *buff, *cache_ptr;
    struct dns_cache *cache_temp;
    long file_size;
    int len, fl=0;

    cache_temp = NULL;
    cache_using = 0;
    if ((cfp = fopen(httpdns.cachePath, "r+")) == NULL) {
        //创建文件并设置权限
        if ((cfp = fopen(httpdns.cachePath, "w")) != NULL) {
            chmod(httpdns.cachePath, S_IWOTH | S_IROTH | S_IWGRP | S_IRGRP | S_IWUSR | S_IRUSR);
            fclose(cfp);
            return 0;
        }
        return 1;
    }

    //读取文件内容
    fseek(cfp, 0, SEEK_END);
    file_size = ftell(cfp);
    if ((buff = (char *)alloca(file_size)) == NULL)
        return 1;
    rewind(cfp);
    if ((fl = fread(buff, file_size, 1, cfp)) != 1)
        ;
    fclose(cfp);

    for (cache_ptr = buff; cache_ptr - buff < file_size; cache_ptr += len) {
        cache_temp = (struct dns_cache *)malloc(sizeof(struct dns_cache));
        if (cache_temp == NULL)
            return 1;
        cache_temp->next = cache;
        cache = cache_temp;
        cache_using++;
        len = *(uint16_t *) cache_ptr + *(uint16_t *) (cache_ptr + *(uint16_t *) cache_ptr + 2) + 4;
        copy_new_mem(cache_ptr, len, &cache->dns_cache);
        if (cache->dns_cache == NULL)
            return 1;
    }
    /* 删除重复记录 */
    struct dns_cache *before, *after;
    for (; cache_temp; cache_temp = cache_temp->next) {
        for (before = cache_temp; before && (after = before->next) != NULL; before = before->next) {
            if (*(uint16_t *) after->dns_cache == *(uint16_t *) cache_temp->dns_cache && memcmp(after->dns_cache + 2, cache_temp->dns_cache + 2, *(uint16_t *) after->dns_cache) == 0) {
                before->next = after->next;
                free(after->dns_cache);
                free(after);
                cache_using--;
            }
        }
    }

    chmod(httpdns.cachePath, S_IWOTH | S_IROTH | S_IWGRP | S_IRGRP | S_IWUSR | S_IRUSR);
    return 0;
}

/* 程序结束时将缓存写入文件 */
static void write_dns_cache(int sig)
{
    //子进程先写入缓存
    if (child_pid) {
        wait(NULL);
        cfp = fopen(httpdns.cachePath, "a");
    } else {
        cfp = fopen(httpdns.cachePath, "w");
    }
    while (cache) {
        fwrite(cache->dns_cache, *(uint16_t *) cache->dns_cache + *(uint16_t *) (cache->dns_cache + *(uint16_t *) cache->dns_cache + 2) + 4, 1, cfp);
        cache = cache->next;
    }

    exit(0);
}

static void dnsProxyStop(dns_t * dns)
{
    epoll_ctl(dns_efd, EPOLL_CTL_DEL, dns->fd, NULL);
    close(dns->fd);
    if (httpdns.tcpDNS_mode == 0)
        free(dns->out_request);
    memset(((char *)dns) + DNS_REQUEST_MAX_SIZE + sizeof(struct sockaddr_in), 0, sizeof(dns_t) - DNS_REQUEST_MAX_SIZE + -sizeof(struct sockaddr_in));
    dns->fd = -1;
}

/* 查询缓存 */
static uint8_t cache_lookup(dns_t * dns)
{
    struct dns_cache *c;
    char *rsp;

    for (c = cache; c; c = c->next) {
        //不匹配dnsID
        if (dns->dns_req_len - 2 == *(uint16_t *) c->dns_cache && memcmp(dns->dns_req + 2, c->dns_cache + 2, dns->dns_req_len - 2) == 0) {
            rsp = c->dns_cache + *(uint16_t *) c->dns_cache + 2;
            *(uint16_t *) (rsp + 2) = *(uint16_t *) dns->dns_req; //设置dns id
            sendto(global.dns_listen_fd, rsp + 2, *(uint16_t *) rsp, 0, (struct sockaddr *)&dns->src_addr, sizeof(struct sockaddr_in));
            return 0;
        }
    }

    return 1;
}

/* 记录缓存 */
static void cache_record(char *request, uint16_t request_len, char *response, uint16_t response_len)
{
    struct dns_cache *cache_temp;

    cache_temp = (struct dns_cache *)malloc(sizeof(struct dns_cache));
    if (cache_temp == NULL)
        return;
    cache_temp->dns_cache = (char *)malloc(request_len + response_len + 2);
    if (cache_temp->dns_cache == NULL) {
        free(cache_temp);
        return;
    }
    cache_temp->next = cache;
    cache = cache_temp;
    //不缓存dnsid
    request += 2;
    request_len -= 2;
    memcpy(cache_temp->dns_cache, &request_len, 2);
    memcpy(cache_temp->dns_cache + 2, request, request_len);
    memcpy(cache_temp->dns_cache + request_len + 2, &response_len, 2);
    memcpy(cache_temp->dns_cache + request_len + 4, response, response_len);
    if (httpdns.cacheLimit) {
        //到达缓存记录条目限制则释放前一半缓存
        if (cache_using >= httpdns.cacheLimit) {
            struct dns_cache *free_c;
            int i;
            for (i = cache_using = httpdns.cacheLimit >> 1; i--; cache_temp = cache_temp->next) ;
            for (free_c = cache_temp->next, cache_temp->next = NULL; free_c; free_c = cache_temp) {
                cache_temp = free_c->next;
                free(free_c->dns_cache);
                free(free_c);
            }
        }
        cache_using++;
    }
}

/* 分析DNS请求 */
static int8_t parse_dns_request(char *dns_req, dns_t * dns)
{
    int len;

    dns_req += 13;              //跳到域名部分
    dns->host_len = strlen(dns_req);
    dns->query_type = *(dns_req + 2 + dns->host_len);
    //tcpdns不需要解析域名
    if (httpdns.tcpDNS_mode == 1)
        return 0;
    //httpdns只支持域名查询ipv4
    if (dns->query_type != 1 || (dns->host = strdup(dns_req)) == NULL)
        return 1;
    for (len = *(--dns_req); dns_req[len + 1] != 0; len += dns_req[len]) {
        //防止数组越界
        if (len > dns->host_len) {
            free(dns->host);
            return 1;
        }
        dns->host[len++] = '.';
    }

    return 0;
}

/* 回应dns客户端 */
static int8_t httpDNS_respond_client(dns_t * dns, char *rspIp)
{
    static char rsp[DNS_REQUEST_MAX_SIZE + 16];
    char *p;
    int rsp_len;

    //18: 查询资源的前(12字节)后(6字节)部分
    rsp_len = 18 + dns->host_len + (rspIp ? 16 : 0);
    //判断是否超出缓冲大小
    if (rsp_len > DNS_REQUEST_MAX_SIZE) {
        dns->query_type = 0;
        return 1;
    }
    /* dns ID */
    memcpy(rsp, dns->dns_req, 2);
    /* 问题数 */
    rsp[4] = 0;
    rsp[5] = 1;
    /* 资源记录数 */
    rsp[6] = 0;
    rsp[7] = 0;
    /* 授权资源记录数 */
    rsp[8] = 0;
    rsp[9] = 0;
    /* 额外资源记录数 */
    rsp[10] = 0;
    rsp[11] = 0;
    memcpy(rsp + 12, dns->dns_req + 12, dns->host_len + 6);
    /* 如果有回应内容(资源记录) */
    if (rspIp) {
        p = rsp + 18 + dns->host_len;
        /* 资源记录数+1 */
        rsp[7]++;
        /* 成功标志 */
        rsp[2] = (char)133;
        rsp[3] = (char)128;
        /* 指向主机域名 */
        p[0] = (char)192;
        p[1] = 12;
        /* 回应类型 */
        p[2] = 0;
        p[3] = dns->query_type;
        /* 区域类别 */
        p[4] = 0;
        p[5] = 1;
        /* 生存时间 (1 ora) */
        p[6] = 0;
        p[7] = 0;
        p[8] = 14;
        p[9] = 16;
        /* 回应长度 */
        p[10] = 0;
        p[11] = 4;
        memcpy(p + 12, rspIp, 4);
    } else {
        /* 失败标志 */
        rsp[2] = (char)129;
        rsp[3] = (char)130;
    }

    //因为UDP是无连接协议，所以不做返回值判断
    sendto(global.dns_listen_fd, rsp, rsp_len, 0, (struct sockaddr *)&dns->src_addr, sizeof(struct sockaddr_in));
    dns->query_type = 0;        //表示结构体空闲
    if (cfp && rspIp)
        cache_record(dns->dns_req, dns->dns_req_len, rsp, rsp_len);
    return 0;
}

void dns_timeout_check()
{
    int i;

    for (i = 0; i < DNS_MAX_CONCURRENT; i++) {
        if (dns_list[i].fd > -1) {
            if (dns_list[i].timer >= global.timeout_m) {
                dnsProxyStop(dns_list + i);
            } else {
                dns_list[i].timer++;
            }
        }
    }
}

static void http_out(dns_t * out)
{
    int write_len;

    out->timer = 0;
    if (httpdns.connect_request && out->sent_CONNECT_len < httpdns.connect_request_len) {
        write_len = write(out->fd, httpdns.connect_request + out->sent_CONNECT_len, httpdns.connect_request_len - out->sent_CONNECT_len);
        if (write_len == -1) {
            dnsProxyStop(out);
        } else {
            out->sent_CONNECT_len += write_len;
            if (out->sent_CONNECT_len == httpdns.connect_request_len) {
                out->sent_CONNECT_len++; //表示已完全发送CONNECT请求，等待HTTP回应
                dns_ev.events = EPOLLIN | EPOLLET;
                dns_ev.data.ptr = out;
                epoll_ctl(dns_efd, EPOLL_CTL_MOD, out->fd, &dns_ev);
            }
        }
        return;
    }

    write_len = write(out->fd, out->out_request, out->out_request_len);
    if (write_len == out->out_request_len) {
        dns_ev.events = EPOLLIN | EPOLLET;
        dns_ev.data.ptr = out;
        epoll_ctl(dns_efd, EPOLL_CTL_MOD, out->fd, &dns_ev);
    } else if (write_len > 0) {
        out->out_request_len -= write_len;
        memmove(out->out_request, out->out_request + write_len, out->out_request_len);
    } else {
        epoll_ctl(dns_efd, EPOLL_CTL_DEL, out->fd, NULL);
        close(out->fd);
        out->query_type = 0;
    }
}

static void handle_httpDNS_rsp(dns_t * dns, char *rsp, int rsp_len)
{
    char *ip_ptr, *p, ip[4];
    int i;

    p = strstr(rsp, "\n\r");
    if (p) {
        p += 3;
        rsp_len -= p - rsp;
        //部分代理服务器使用长连接，第二次读取数据才读到域名的IP
        if (rsp_len <= 0)
            return;
        rsp = p;
    }
    if (httpdns.encodeCode)
        dataEncode(rsp, rsp_len, httpdns.encodeCode);
    do {
        if (*rsp == '\n')
            rsp++;
        /* 匹配IP */
        while ((*rsp > 57 || *rsp < 49) && *rsp != '\0')
            rsp++;
        for (i = 0, ip_ptr = rsp, rsp = strchr(ip_ptr, '.');; ip_ptr = rsp + 1, rsp = strchr(ip_ptr, '.')) {
            if (i < 3) {
                if (rsp == NULL) {
                    httpDNS_respond_client(dns, NULL);
                    return;
                }
                //查找下一行
                if (rsp - ip_ptr > 3)
                    break;
                ip[i++] = atoi(ip_ptr);
            } else {
                ip[3] = atoi(ip_ptr);
                httpDNS_respond_client(dns, ip);
                return;
            }
        }
    } while ((rsp = strchr(rsp, '\n')) != NULL);
}

static void handle_tcpDNS_rsp(dns_t * dns, char *rsp, int rsp_len)
{
    /* 转换为UDPdns请求(为什么不做长度判断？因为懒) */
    rsp += 2;
    rsp_len -= 2;
    //回应客户端
    sendto(global.dns_listen_fd, rsp, rsp_len, 0, (struct sockaddr *)&dns->src_addr, sizeof(struct sockaddr_in));
    if (cfp && (unsigned char)rsp[3] == 128) {
        //如果使用编码，则需要还原dns请求
        if (httpdns.httpsProxy_encodeCode)
            dataEncode(dns->dns_req + 2 /* 换成TCPDNS请求时原本请求后移动了2字节 */ , dns->dns_req_len, httpdns.httpsProxy_encodeCode);
        cache_record(dns->dns_req + 2, dns->dns_req_len, rsp, (uint16_t) rsp_len);
    }
}

static void http_in(dns_t * in)
{
    static char http_rsp[HTTP_RSP_SIZE + 1];
    int len;

    in->timer = 0;
    if (httpdns.connect_request && in->sent_CONNECT_len > httpdns.connect_request_len) {
        in->sent_CONNECT_len--;
        do {
            len = read(in->fd, http_rsp, HTTP_RSP_SIZE);
            //没有数据读取，CONNECT代理连接完成
            if (len < 0 && errno == EAGAIN)
                break;
            if (len <= 0) {
                dnsProxyStop(in);
                return;
            }
        } while (len == HTTP_RSP_SIZE);
        dns_ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        dns_ev.data.ptr = in;
        epoll_ctl(dns_efd, EPOLL_CTL_MOD, in->fd, &dns_ev);
        return;
    }
    len = read(in->fd, http_rsp, HTTP_RSP_SIZE);
    if (len <= 0) {
        if (len == 0 || errno != EAGAIN)
            dnsProxyStop(in);
        return;
    }
    http_rsp[len] = '\0';
    if (httpdns.httpsProxy_encodeCode)
        dataEncode(http_rsp, len, httpdns.httpsProxy_encodeCode);
    if (httpdns.tcpDNS_mode == 1) {
        handle_tcpDNS_rsp(in, http_rsp, len);
    } else {
        handle_httpDNS_rsp(in, http_rsp, len);
    }
    dnsProxyStop(in);
}

static int8_t create_outRequest(dns_t * dns)
{
    if (parse_dns_request(dns->dns_req, dns) != 0) {
        dns->out_request = NULL;
        return 1;
    }
    if (httpdns.tcpDNS_mode == 1) {
        memmove(dns->dns_req + 2, dns->dns_req, dns->dns_req_len);
        dns->dns_req[0] = *((char *)(&dns->dns_req_len) + 1);
        dns->dns_req[1] = *((char *)&dns->dns_req_len);
        dns->out_request = dns->dns_req;
        dns->out_request_len = dns->dns_req_len + 2;
        dns->host = NULL;
        /*
           //调试用
           int i;
           printf("(");
           for (i=0;i<dns->out_request_len;i++)
           printf("%u ", dns->out_request[i]);
           puts(")");
         */
    } else {
        if (httpdns.encodeCode)
            dataEncode(dns->host, dns->host_len, httpdns.encodeCode);
        dns->out_request_len = httpdns.http_req_len;
        copy_new_mem(httpdns.http_req, httpdns.http_req_len, &dns->out_request);
        dns->out_request = replace(dns->out_request, &dns->out_request_len, "[D]", 3, dns->host, dns->host_len);
        free(dns->host);
        if (dns->out_request == NULL)
            return 1;
    }
    if (httpdns.httpsProxy_encodeCode)
        dataEncode(dns->out_request, dns->out_request_len, httpdns.httpsProxy_encodeCode);
    dns->sent_CONNECT_len = 0;

    return 0;
}

/* 连接到dns服务器 */
static int connectToDnsServer(dns_t * dns)
{
    dns->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (dns->fd < 0)
        return 1;
    dns->timer = 0;             //超时计时器设为0
    fcntl(dns->fd, F_SETFL, O_NONBLOCK);
    dns_ev.events = EPOLLERR | EPOLLOUT | EPOLLET;
    dns_ev.data.ptr = dns;
    if (epoll_ctl(dns_efd, EPOLL_CTL_ADD, dns->fd, &dns_ev) != 0) {
        close(dns->fd);
        return 1;
    }
    if (connect(dns->fd, (struct sockaddr *)&httpdns.dst, sizeof(httpdns.dst)) != 0 && errno != EINPROGRESS) {
        epoll_ctl(dns_efd, EPOLL_CTL_DEL, dns->fd, NULL);
        close(dns->fd);
        return 1;
    }

    return 0;
}

static void new_client()
{
    dns_t *dns;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int i, len;

    for (i = 0; i < DNS_MAX_CONCURRENT; i++) {
        if (dns_list[i].query_type == 0) {
            dns = &dns_list[i];
            break;
        }
    }
    if (i == DNS_MAX_CONCURRENT)
        return;
    len = recvfrom(global.dns_listen_fd, dns->dns_req, DNS_REQUEST_MAX_SIZE - 2, 0, (struct sockaddr *)&dns->src_addr, &addr_len);
    //dns请求必须大于18
    if (len <= 18)
        return;
    dns->dns_req_len = (uint16_t) len;
    /* 查询缓存 */
    if (cfp && cache_lookup(dns) == 0)
        return;
    if (create_outRequest(dns) != 0 || connectToDnsServer(dns) != 0) {
        if (dns->out_request != dns->dns_req)
            free(dns->out_request);
        httpDNS_respond_client(dns, NULL);
    }
}

static void httpRequest_init()
{
    char dest[22];
    uint16_t port;

    port = ntohs(httpdns.dst.sin_port);
    sprintf(dest, "%s:%u", inet_ntoa(httpdns.dst.sin_addr), port);

    //如果设置http_req = "";则不创建请求头
    if (httpdns.http_req_len > 0) {
        httpdns.http_req_len = strlen(httpdns.http_req);
        httpdns.http_req_len += 2;
        httpdns.http_req = (char *)realloc(httpdns.http_req, httpdns.http_req_len + 1);
        if (httpdns.http_req == NULL)
            printf("out of memory.\n");
        strcat(httpdns.http_req, "\r\n");
        httpdns.http_req = replace(httpdns.http_req, &httpdns.http_req_len, "[V]", 3, "HTTP/1.1", 8);
        httpdns.http_req = replace(httpdns.http_req, &httpdns.http_req_len, "[H]", 3, dest, strlen(dest));
        httpdns.http_req = replace(httpdns.http_req, &httpdns.http_req_len, "\\0", 2, "\0", 1);
        if (httpdns.tcpDNS_mode == 0) {
            httpdns.http_req = replace(httpdns.http_req, &httpdns.http_req_len, "[M]", 3, "GET", 3);
            httpdns.http_req = replace(httpdns.http_req, &httpdns.http_req_len, "[url]", 5, "/d?dn=[D]", 9);
            httpdns.http_req = replace(httpdns.http_req, &httpdns.http_req_len, "[U]", 3, "/d?dn=[D]", 9);
            if (httpdns.http_req == NULL)
                printf("out of memory.\n");
        } else {
            httpdns.http_req = replace(httpdns.http_req, &httpdns.http_req_len, "[M]", 3, "CONNECT", 7);
            httpdns.http_req = replace(httpdns.http_req, &httpdns.http_req_len, "[url]", 5, "/", 1);
            httpdns.http_req = replace(httpdns.http_req, &httpdns.http_req_len, "[U]", 3, "/", 1);
            if (httpdns.http_req == NULL)
                printf("out of memory.\n");
            httpdns.connect_request = httpdns.http_req;
            httpdns.connect_request_len = httpdns.http_req_len;
            httpdns.http_req = NULL;
        }
    }
}

void dns_init()
{
    httpRequest_init();
    dns_efd = epoll_create(DNS_MAX_CONCURRENT + 1);
    if (dns_efd < 0) {
        perror("epoll_create");
        exit(1);
    }
    fcntl(global.dns_listen_fd, F_SETFL, O_NONBLOCK);
    dns_ev.data.fd = global.dns_listen_fd;
    dns_ev.events = EPOLLIN;
    epoll_ctl(dns_efd, EPOLL_CTL_ADD, global.dns_listen_fd, &dns_ev);
    memset(dns_list, 0, sizeof(dns_list));
    //程序关闭时写入dns缓存
    if (cfp) {
        signal(SIGTERM, write_dns_cache);
        signal(SIGHUP, write_dns_cache);
        signal(SIGINT, write_dns_cache);
        signal(SIGABRT, write_dns_cache);
        signal(SIGILL, write_dns_cache);
        signal(SIGSEGV, write_dns_cache);
    }
}

void *dns_loop(void *nullPtr)
{
    int n;

    while (1) {
        n = epoll_wait(dns_efd, dns_evs, DNS_MAX_CONCURRENT + 1, -1);

        while (n-- > 0) {
            if (dns_evs[n].data.fd == global.dns_listen_fd) {
                new_client();
            } else {
                if (dns_evs[n].events & EPOLLIN) {
                    http_in((dns_t *) dns_evs[n].data.ptr);
                }
                if (dns_evs[n].events & EPOLLOUT) {
                    http_out((dns_t *) dns_evs[n].data.ptr);
                }
            }
        }
    }

    return NULL;                //消除编译警告
}
