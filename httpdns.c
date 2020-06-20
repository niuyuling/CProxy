#include "httpdns.h"

int encodeCode=0;
/* hosts变量 */
char *hosts_path = NULL;
FILE *hostsfp = NULL;
struct dns_hosts *hosts, *last_hosts = NULL;

/* encode domain and ipAddress */
static void dataEncode(unsigned char *data, int data_len)
{
    while (data_len-- > 0)
        data[data_len] ^= encodeCode;
}

int read_hosts_file(char *path)
{
    char *ip_begin, *ip_end, *host_begin, *host_end, *buff, *next_line;
    int file_size, i;

    hosts = last_hosts = NULL;
    if ((hostsfp = fopen(path, "r")) == NULL) {
        fputs("error hosts file path", stderr);
        return 1;
    }
    //读取文件内容
    fseek(hostsfp, 0, SEEK_END);
    file_size = ftell(hostsfp);
    //文件没有内容则不用读取
    if (file_size == 0)
        return 0;
    if ((buff = (char *)alloca(file_size + 1)) == NULL) {
        fclose(hostsfp);
        fputs("out of memory", stderr);
        return 1;
    }
    rewind(hostsfp);
    fread(buff, file_size, 1, hostsfp);
    *(buff + file_size) = '\0';
    fclose(hostsfp);

    struct dns_hosts *h = NULL;
    for (ip_begin = buff; ip_begin; ip_begin = next_line) {
        next_line = strchr(ip_begin, '\n');
        if (next_line != NULL)
            *next_line++ = '\0';
        while (*ip_begin == '\t' || *ip_begin == ' ' || *ip_begin == '\r')
            if (*ip_begin++ == '\0')
                continue;
        for (i = 0, ip_end = ip_begin; *ip_end != ' ' && *ip_end != '\t' && *ip_end != '\r' && *ip_end != '\0'; ip_end++) {
            if (*ip_end == '.')
                i++;
            else if (*ip_end == '\0')
                continue;
        }
        if (i != 3)
            continue;
        for (host_begin = ip_end; *host_begin == '\t' || *host_begin == ' ' || *host_begin == '\r';) {
            if (*host_begin++ == '\0')
                continue;
        }
        for (host_end = host_begin; *host_end != ' ' && *host_end != '\t' && *host_end != '\r' && *host_end != '\n' && *host_end != '\0'; host_end++) ;
        if (h) {
            h->next = (struct dns_hosts *)malloc(sizeof(struct dns_hosts));
            if (h->next == NULL)
                return 1;
            h = h->next;
        } else {
            hosts = h = (struct dns_hosts *)malloc(sizeof(struct dns_hosts));
            if (hosts == NULL) {
                fputs("out of memory", stderr);
                return 1;
            }
        }
        h->next = NULL;
        h->ip = strndup(ip_begin, ip_end - ip_begin);
        if (*(host_end - 1) == '.')
            host_end--;
        h->host = strndup(host_begin, host_end - host_begin);
        if (h->ip == NULL || h->host == NULL) {
            fputs("out of memory", stderr);
            return 1;
        }

    }

    last_hosts = h;
    return 0;
}

char *hosts_lookup(char *host)
{
    struct dns_hosts *h;

    h = hosts;
    while (h) {
        if (strcmp(h->host, host) == 0)
            return h->ip;
        h = h->next;
    }

    return NULL;
}

void close_client(dns_t * dns)
{
    close(dns->fd);
    if (dns->http_rsp_len != sizeof(ERROR_MSG) - 1) //ERROR_MSG not free()
        free(dns->http_rsp);
    dns->http_rsp = NULL;
    dns->sent_len = dns->dns_req_len = 0;
    dns->fd = -1;
}

void build_http_rsp(dns_t * dns, char *ips)
{
    int ips_len = strlen(ips);

    if (encodeCode)
        dataEncode((unsigned char *)ips, ips_len);
    dns->http_rsp_len = sizeof(SUCCESS_HEADER) - 1 + ips_len;
    dns->http_rsp = (char *)malloc(dns->http_rsp_len + 1);
    if (dns->http_rsp == NULL)
        return;
    strcpy(dns->http_rsp, SUCCESS_HEADER);
    memcpy(dns->http_rsp + sizeof(SUCCESS_HEADER) - 1, ips, ips_len);
    dns->sent_len = 0;
}

void response_client(dns_t * out)
{
    int write_len = write(out->fd, out->http_rsp + out->sent_len, out->http_rsp_len - out->sent_len);
    if (write_len == out->http_rsp_len - out->sent_len || write_len == -1)
        close_client(out);
    else
        out->sent_len += write_len;
}

void build_dns_req(dns_t * dns, char *domain, int domain_size)
{
    char *p, *_p;

    p = dns->dns_req + 12;
    memcpy(p + 1, domain, domain_size + 1); //copy '\0'
    while ((_p = strchr(p + 1, '.')) != NULL) {
        *p = _p - p - 1;
        p = _p;
    }
    *p = strlen(p + 1);
    p = dns->dns_req + 14 + domain_size;
    *p++ = 0;
    *p++ = 1;
    *p++ = 0;
    *p++ = 1;
    dns->dns_req_len = p - dns->dns_req;
}

int send_dns_req(char *dns_req, int req_len)
{
    int write_len;

    write_len = write(dstFd, dns_req, req_len);
    if (write_len == req_len)
        return 0;
    return write_len;
}

void query_dns()
{
    dns_t *dns;
    int i, ret;

    for (i = MAX_FD - 2, dns = &dns_list[MAX_FD - 3]; i--; dns--) {
        if (dns->http_rsp == NULL && dns->dns_req_len != dns->sent_len) {
            ret = send_dns_req(dns->dns_req + dns->sent_len, dns->dns_req_len - dns->sent_len);
            if (ret == 0) {
                dns->sent_len = dns->dns_req_len;
            } else if (ret > -1) {
                dns->sent_len += ret;
                return;
            } else {
                close_client(dns);
                break;
            }
        }
    }
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = dstFd;
    epoll_ctl(eFd, EPOLL_CTL_MOD, dstFd, &ev);
}

void recv_dns_rsp()
{
    static char rsp_data[BUFF_SIZE + 1], *p, *ips, *ips_save;
    unsigned char *_p;
    dns_t *dns;
    int len, ips_len;
    int16_t flag;

    len = read(dstFd, rsp_data, BUFF_SIZE);
    if (len < 2)
        return;
    memcpy(&flag, rsp_data, sizeof(int16_t));
    if (flag > MAX_FD - 3)
        return;
    dns = dns_list + flag;
    dns->sent_len = 0;
    dns->http_rsp = ERROR_MSG;
    dns->http_rsp_len = sizeof(ERROR_MSG) - 1;
    if (dns->dns_req_len + 12 > len || (unsigned char)rsp_data[3] != 128 /*(signed char) max is 127 */ )
        goto modEvToOut;
    rsp_data[len] = '\0';
    /* get ips */
    p = rsp_data + dns->dns_req_len + 11;
    ips_len = 0;
    ips = NULL;
    while (p - rsp_data + 4 <= len) {
        //type
        if (*(p - 8) != 1) {
            p += *p + 12;
            continue;
        }
        ips_save = ips;
        ips = (char *)realloc(ips, ips_len + 16);
        if (ips == NULL) {
            ips = ips_save;
            break;
        }
        _p = (unsigned char *)p + 1;
        ips_len += sprintf(ips + ips_len, "%d.%d.%d.%d", _p[0], _p[1], _p[2], _p[3]);
        p += 16;                //next address
        ips[ips_len++] = ';';
    }
    if (ips) {
        ips[ips_len - 1] = '\0';
        //printf("ips %s\n", ips);
        build_http_rsp(dns, ips);
        free(ips);
        if (dns->http_rsp) {
            response_client(dns);
            if (dns->http_rsp == NULL) {
                dns->http_rsp = ERROR_MSG;
                dns->http_rsp_len = sizeof(ERROR_MSG) - 1;
            }
        }
    }
modEvToOut:
    ev.data.ptr = dns;
    ev.events = EPOLLOUT | EPOLLET;
    epoll_ctl(eFd, EPOLL_CTL_MOD, dns->fd, &ev);
}

void read_client(dns_t * in)
{
    static char httpReq[BUFF_SIZE + 1];
    int domain_size, httpReq_len;
    char *domain_begin, *domain_end, *domain = NULL, *ips;

    httpReq_len = read(in->fd, httpReq, BUFF_SIZE);
    //必须大于5，否则不处理
    if (httpReq_len < 6) {
        close_client(in);
        return;
    }
    httpReq[httpReq_len] = '\0';
    in->http_rsp = ERROR_MSG;
    in->http_rsp_len = sizeof(ERROR_MSG) - 1;
    if ((domain_begin = strstr(httpReq, "?dn=")))
        domain_begin += 4;
    else if ((domain_begin = strstr(httpReq, "?host=")))
        domain_begin += 6;
    else
        goto response_client;

    domain_end = strchr(domain_begin, ' ');
    if (domain_end == NULL)
        goto response_client;
    if (*(domain_end - 1) == '.')
        domain_size = domain_end - domain_begin - 1;
    else
        domain_size = domain_end - domain_begin;
    domain = strndup(domain_begin, domain_size);
    if (encodeCode)
        dataEncode((unsigned char *)domain, domain_size);
    if (domain == NULL || domain_size <= 0)
        goto response_client;
    if (hostsfp && (ips = hosts_lookup(domain)) != NULL) {
        free(domain);
        build_http_rsp(in, ips);
        if (in->http_rsp == NULL) {
            in->http_rsp = ERROR_MSG;
            in->http_rsp_len = sizeof(ERROR_MSG) - 1;
        }
    } else {
        build_dns_req(in, domain, domain_size);
        free(domain);
        int ret = send_dns_req(in->dns_req, in->dns_req_len);
        switch (ret) {
        case 0:
            in->sent_len = in->dns_req_len;
            ev.events = EPOLLIN;
            break;

        case -1:
            close_client(in);
            return;

        default:
            in->sent_len += ret;
            ev.events = EPOLLIN | EPOLLOUT;
            break;
        }
        ev.data.fd = dstFd;
        epoll_ctl(eFd, EPOLL_CTL_MOD, dstFd, &ev);
        return;
    }

response_client:
    response_client(in);
    if (in->http_rsp) {
        ev.data.ptr = in;
        ev.events = EPOLLOUT | EPOLLET;
        epoll_ctl(eFd, EPOLL_CTL_MOD, in->fd, &ev);
    }
}

void httpdns_accept_client()
{
    struct sockaddr_in addr;
    dns_t *client;
    int i;

    for (i = MAX_FD - 2; i--;) {
        if (dns_list[i].fd < 0) {
            client = &dns_list[i];
            break;
        }
    }
    //printf("i = %d\n" , i);
    if (i < 0)
        return;
    client->fd = accept(listenFd, (struct sockaddr *)&addr, &addr_len);
    if (client->fd < 0) {
        return;
    }
    fcntl(client->fd, F_SETFL, O_NONBLOCK);
    ev.data.ptr = client;
    ev.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(eFd, EPOLL_CTL_ADD, client->fd, &ev) != 0) {
        close(client->fd);
        client->fd = -1;
        return;
    }
}

int httpdns_initialize()
{
    struct sockaddr_in listenAddr, dnsAddr;
    int optval = 0;

    //ignore PIPE signal
    signal(SIGPIPE, SIG_IGN);
    dnsAddr.sin_family = listenAddr.sin_family = AF_INET;   // IPv4
    dnsAddr.sin_addr.s_addr = inet_addr(DEFAULT_UPPER_IP);  // 本地监听
    dnsAddr.sin_port = htons(53);

    listenAddr.sin_addr.s_addr = INADDR_ANY;
    listenAddr.sin_port = htons(53);
    
    listenFd = socket(AF_INET, SOCK_STREAM, 0);     // 本地监听,TCP协议
    dstFd = socket(AF_INET, SOCK_DGRAM, 0);         // DNS监听,UDP协议
    if (dstFd < 0 || listenFd < 0) {
        perror("socket");
        return 1;
    }
    fcntl(dstFd, F_SETFL, O_NONBLOCK);              // dstFd 非阻塞
    fcntl(listenFd, F_SETFL, O_NONBLOCK);           // listenFd 非阻塞

    optval = 1;
    if (setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) != 0) {
        perror("setsockopt");
        return 1;
    }
    if (bind(listenFd, (struct sockaddr *)&listenAddr, sizeof(listenAddr)) != 0) {
        perror("bind");
        return 1;
    }
    if (listen(listenFd, 20) != 0) {
        perror("listen");
        return 1;
    }
    
    eFd = epoll_create(MAX_FD - 1);
    if (eFd < 0) {
        perror("epoll_create");
        return 1;
    }
    connect(dstFd, (struct sockaddr *)&dnsAddr, sizeof(dnsAddr));
    ev.data.fd = listenFd;
    ev.events = EPOLLIN;
    epoll_ctl(eFd, EPOLL_CTL_ADD, listenFd, &ev);
    ev.data.fd = dstFd;
    ev.events = EPOLLIN;
    epoll_ctl(eFd, EPOLL_CTL_ADD, dstFd, &ev);
    memset(dns_list, 0, sizeof(dns_list));
    //初始化DNS请求结构
    int16_t i;
    for (i = MAX_FD - 2; i--;) {
        dns_list[i].fd = -1;
        memcpy(dns_list[i].dns_req, &i, sizeof(i));
        dns_list[i].dns_req[2] = 1;
        dns_list[i].dns_req[3] = 0;
        dns_list[i].dns_req[4] = 0;
        dns_list[i].dns_req[5] = 1;
        dns_list[i].dns_req[6] = 0;
        dns_list[i].dns_req[7] = 0;
        dns_list[i].dns_req[8] = 0;
        dns_list[i].dns_req[9] = 0;
        dns_list[i].dns_req[10] = 0;
        dns_list[i].dns_req[11] = 0;
    }

    return 0;
}

void *httpdns_start_server(void *p)
{
    int n;

    while (1) {
        n = epoll_wait(eFd, evs, MAX_FD - 1, -1);
        //printf("n = %d\n", n);
        while (n-- > 0) {
            if (evs[n].data.fd == listenFd) {
                httpdns_accept_client();
            } else if (evs[n].data.fd == dstFd) {
                if (evs[n].events & EPOLLIN) {
                    recv_dns_rsp();
                } else if (evs[n].events & EPOLLOUT) {
                    query_dns();
                }
            } else if (evs[n].events & EPOLLIN) {
                read_client(evs[n].data.ptr);
            } else if (evs[n].events & EPOLLOUT) {
                response_client(evs[n].data.ptr);
            }
        }
    }
    return NULL;
}
