/*
    HTTPUDP模块代理UDP过程:
        获取客户端UDP数据
        向服务器发送一个http请求头
        收到服务端回应后发送数据到服务端，内容为: UDP原始目标地址[struct in_addr](只有第一个数据包发送) + UDP长度[uint16_t] + UDP真实数据
        服务端返回数据，数据内容为: UDP包的长度[uint16_t] + UDP真实数据
        新建一个socket伪装原目标地址向客户端发送返回的数据(此功能需要root，否则部分UDP代理不上，例如QQ语音)
*/
#include "http_request.h"
#include "httpudp.h"

#define MAX_CLIENT_INFO 512
#define HTTP_RSP_SIZE 2048
#define CLIENT_BUFFER_SIZE 65535 //如果可以   尽量一次性读完数据
#define SERVER_BUFFER_SIZE 8192

typedef struct connection_info {
    char client_data[CLIENT_BUFFER_SIZE + sizeof(struct sockaddr_in) + sizeof(uint16_t)];
    struct sockaddr_in inaddr, toaddr;
    struct connection_info *next;
    char *rsp_data;
    int client_data_len, client_data_sent_len, http_request_sent_len, rsp_data_len, rsp_data_sent_len, server_fd, responseClientFd, timer;
} info_t;

static info_t client_info_list[MAX_CLIENT_INFO];
static struct epoll_event udp_evs[MAX_CLIENT_INFO * 2 + 2], udp_ev;
struct httpudp udp;
static int udp_efd;

static void proxyStop(info_t * info)
{
    epoll_ctl(udp_efd, EPOLL_CTL_DEL, info->server_fd, NULL);
    epoll_ctl(udp_efd, EPOLL_CTL_DEL, info->responseClientFd, NULL);
    close(info->server_fd);
    close(info->responseClientFd);
    free(info->rsp_data);
    info->rsp_data = NULL;
    do {
        info->server_fd = info->responseClientFd = -1;
        info->rsp_data_len = info->rsp_data_sent_len = info->client_data_sent_len = info->http_request_sent_len = 0;
    } while ((info = info->next) != NULL);
}

void udp_timeout_check()
{
    int i;

    for (i = 0; i < MAX_CLIENT_INFO; i++) {
        if (client_info_list[i].server_fd > -1) {
            if (client_info_list[i].timer >= global.timeout_m)
                proxyStop(client_info_list + i);
            else
                client_info_list[i].timer = 0;
        }
    }
}

/* 创建udpfd回应客户端 */
static int createRspFd(info_t * client)
{
    int opt = 1;

    client->responseClientFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (client->responseClientFd < 0)
        return 1;
    fcntl(client->responseClientFd, F_SETFL, O_NONBLOCK);
    /*
       以下函数不做返回值判断
       因为有些UDP客户端不需要伪装源目标地址
     */
    setsockopt(client->responseClientFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(client->responseClientFd, SOL_IP, IP_TRANSPARENT, &opt, sizeof(opt));
    //切换root伪装源目标地址
    if (-1 == seteuid(0))
        perror("seteuid");
    if (-1 == setegid(0))
        perror("setegid");
    bind(client->responseClientFd, (struct sockaddr *)&client->toaddr, sizeof(struct sockaddr_in));
    //切换回用户设置的uid
    if (-1 == setegid(global.uid))
         perror("setegid");
    if (-1 == seteuid(global.uid))
         perror("seteuid");

    return 0;
}

/* 将服务端返回的数据发送到客户端 */
static int outputToClient(info_t * client)
{
    char *dataPtr;
    int write_len;

    if (client->responseClientFd < 0 && createRspFd(client) < 0)
        return 1;

    client->timer = 0;
    dataPtr = client->rsp_data;
    //至少要有一个完整的udp包才返回客户端
    while ((int)(*(uint16_t *) dataPtr + sizeof(uint16_t)) <= client->rsp_data_len) {
        write_len = sendto(client->responseClientFd, dataPtr + sizeof(uint16_t) + client->rsp_data_sent_len, *(uint16_t *) dataPtr - client->rsp_data_sent_len, 0, (struct sockaddr *)&client->inaddr, sizeof(struct sockaddr_in));
        //printf("rsp: [write_len:%d, dataLen:%u, sent:%u, total:%d]\n", write_len, *(uint16_t *)dataPtr, client->rsp_data_sent_len, client->rsp_data_len);
        if (write_len < 0 && errno == EAGAIN)
            return 0;
        client->rsp_data_sent_len += write_len;
        if (write_len == 0 || write_len < 0) {
            //perror("toClient write()");
            return 1;
        }
        if (write_len < *(uint16_t *) dataPtr) {
            udp_ev.data.ptr = client;
            udp_ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
            epoll_ctl(udp_efd, EPOLL_CTL_ADD, client->responseClientFd, &udp_ev);
            return 0;
        }
        dataPtr += write_len + sizeof(uint16_t);
        client->rsp_data_len -= write_len + sizeof(uint16_t);
        client->rsp_data_sent_len = 0;
    }
    //发送完已读取到的所有数据  释放内存
    if (client->rsp_data_len == 0) {
        free(client->rsp_data);
        client->rsp_data = NULL;
        udp_ev.data.ptr = client;
        udp_ev.events = EPOLLIN | EPOLLET;
        epoll_ctl(udp_efd, EPOLL_CTL_MOD, client->responseClientFd, &udp_ev);
    }
    //还有数据未返回给客户端，将未返回的数据复制到内存头
    else if (dataPtr > client->rsp_data) {
        memmove(client->rsp_data, dataPtr, client->rsp_data_len);
    }

    return 0;
}

/* 读取服务器的数据并返回给客户端 */
static void recvServer(info_t * in)
{
    in->timer = 0;
    //当条件成立时表示未接收https回应状态码
    if (udp.http_request_len == in->http_request_sent_len) {
        static char http_rsp[HTTP_RSP_SIZE];
        int read_len;
        do {
            read_len = read(in->server_fd, http_rsp, HTTP_RSP_SIZE);
            if (read_len == 0 || (read_len < 0 && errno != EAGAIN)) {
                proxyStop(in);
                return;
            }
        } while (read_len == HTTP_RSP_SIZE);
        in->http_request_sent_len++; //不再接收http头
        udp_ev.data.ptr = in;
        udp_ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        epoll_ctl(udp_efd, EPOLL_CTL_MOD, in->server_fd, &udp_ev);
        return;
    }

    char *new_data;
    int read_len;
    do {
        new_data = (char *)realloc(in->rsp_data, in->rsp_data_len + SERVER_BUFFER_SIZE);
        if (new_data == NULL) {
            proxyStop(in);
            return;
        }
        in->rsp_data = new_data;
        read_len = read(in->server_fd, in->rsp_data + in->rsp_data_len, SERVER_BUFFER_SIZE);
        /* 判断是否关闭连接 */
        if (read_len <= 0) {
            if (read_len == 0 || errno != EAGAIN || in->rsp_data_len == 0) {
                proxyStop(in);
                return;
            }
            read_len = 0;
            break;
        }
        if (udp.httpsProxy_encodeCode)
            dataEncode(in->rsp_data + in->rsp_data_len, read_len, udp.httpsProxy_encodeCode);
        if (udp.encodeCode)
            dataEncode(in->rsp_data + in->rsp_data_len, read_len, udp.encodeCode);
        in->rsp_data_len += read_len;
    } while (read_len == SERVER_BUFFER_SIZE);
    outputToClient(in);
}

/* 向服务器发送数据 */
static int sendToServer(info_t * out)
{
    info_t *send_info;
    int len;

    out->timer = 0;
    /* 发送http请求头到服务器 */
    if (udp.http_request_len > out->http_request_sent_len) {
        len = write(out->server_fd, udp.http_request + out->http_request_sent_len, udp.http_request_len - out->http_request_sent_len);
        if (len <= 0) {
            if (len == 0 || errno != EAGAIN)
                return 1;
            return 0;
        }
        if (len > 0) {
            out->http_request_sent_len += len;
            if (udp.http_request_len == out->http_request_sent_len) {
                udp_ev.data.ptr = out;
                udp_ev.events = EPOLLIN | EPOLLET;
                epoll_ctl(udp_efd, EPOLL_CTL_MOD, out->server_fd, &udp_ev);
            }
        }
        return 0;
    }

    /* 发送UDP目标地址,UDP数据长度和UDP真实数据到服务器 */
    for (send_info = out; send_info; send_info = send_info->next) {
        if (send_info->client_data_len == send_info->client_data_sent_len)
            continue;

        len = write(out->server_fd, send_info->client_data + send_info->client_data_sent_len, send_info->client_data_len - send_info->client_data_sent_len);
        //printf("server_fd: %d, write_len: %d, udp_len: %d, sent_le: %d\n", out->server_fd, len, send_info->client_data_len - send_info->client_data_sent_len, send_info->client_data_sent_len);
        if (len <= 0) {
            if (len == 0 || errno != EAGAIN)
                return 1;
            break;
        }
        send_info->client_data_sent_len += len;
        if (send_info->client_data_sent_len < send_info->client_data_len)
            break;
        if (send_info != out) {
            //此结构体已用完
            send_info->server_fd = -1;
            send_info->client_data_sent_len = 0;
        }
    }
    if (send_info == NULL) {
        udp_ev.data.ptr = out;
        udp_ev.events = EPOLLIN | EPOLLET;
        epoll_ctl(udp_efd, EPOLL_CTL_MOD, out->server_fd, &udp_ev);
    }
    out->next = send_info;

    return 0;
}

static void outEvent(info_t * out)
{
    if (out->server_fd == -1)
        return;

    if ((out->rsp_data && outputToClient(out) != 0) || sendToServer(out) != 0)
        proxyStop(out);
}

static int recvClient(info_t * client)
{
    static char control[1024];
    struct msghdr msg;
    struct iovec io;
    struct cmsghdr *cmsg;

    msg.msg_name = &client->inaddr;
    msg.msg_namelen = sizeof(client->inaddr);
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);
    io.iov_base = client->client_data + sizeof(struct sockaddr_in) + sizeof(uint16_t);
    io.iov_len = CLIENT_BUFFER_SIZE;
    client->client_data_len = recvmsg(global.udp_listen_fd, &msg, 0);
    if (client->client_data_len <= 0) {
        //perror("recvmsg()");
        return 1;
    }
    /* 取得客户端目标地址 */
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg))
        if (cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_ORIGDSTADDR && cmsg->cmsg_len >= CMSG_LEN(sizeof(client->toaddr))) {
            memcpy(&client->toaddr, CMSG_DATA(cmsg), sizeof(client->toaddr));
            break;
        }
    if (cmsg == NULL)
        return 1;
    /*
       printf("src ip: [%s], port: [%d]\n", inet_ntoa(client->inaddr.sin_addr), ntohs(client->inaddr.sin_port));
       printf("dst ip: [%s], port: [%d]\n", inet_ntoa(client->toaddr.sin_addr), ntohs(client->toaddr.sin_port));
     */
    //printf("client len: %d\n", client->client_data_len);
    //复制udp长度和原始目标地址
    memcpy(client->client_data, &client->toaddr, sizeof(struct sockaddr_in));
    memcpy(client->client_data + sizeof(struct sockaddr_in), &client->client_data_len, sizeof(uint16_t));
    client->client_data_len += sizeof(uint16_t) + sizeof(struct sockaddr_in);
    if (udp.encodeCode)
        dataEncode(client->client_data, client->client_data_len, udp.encodeCode);
    if (udp.httpsProxy_encodeCode)
        dataEncode(client->client_data, client->client_data_len, udp.httpsProxy_encodeCode);

    client->next = NULL;

    return 0;
}

static void connectToServer(info_t * info)
{
    info->timer = 0;
    info->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (info->server_fd < 0)
        return;
    fcntl(info->server_fd, F_SETFL, O_NONBLOCK);
    udp_ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
    udp_ev.data.ptr = info;
    if (epoll_ctl(udp_efd, EPOLL_CTL_ADD, info->server_fd, &udp_ev) != 0) {
        close(info->server_fd);
        info->server_fd = -1;
    } else if (connect(info->server_fd, (struct sockaddr *)&udp.dst, sizeof(udp.dst)) != 0 && errno != EINPROGRESS) {
        epoll_ctl(udp_efd, EPOLL_CTL_DEL, info->server_fd, NULL);
        close(info->server_fd);
        info->server_fd = -1;
    }
}

/* 源地址跟目标地址一样的话，服务端需要同一个socket转发 */
static int margeClient(info_t * client)
{
    int i;

    for (i = 0; i < MAX_CLIENT_INFO; i++) {
        if (client != client_info_list + i && client_info_list[i].server_fd > -1 && memcmp(((char *)&client->toaddr) + 2, ((char *)&client_info_list[i].toaddr) + 2, 6) == 0 && memcmp(((char *)&client->inaddr) + 2, ((char *)&client_info_list[i].inaddr) + 2, 6) == 0) {
            info_t *lastInfo;
            for (lastInfo = client_info_list + i; lastInfo->next; lastInfo = lastInfo->next) ;
            lastInfo->next = client;
            client->server_fd = -2; //保证下次调用margeClient()不匹配到这个结构体  并且不被其他客户端连接使用
            client->client_data_sent_len = sizeof(struct sockaddr_in); //不再发送UDP目标地址
            //没有收到服务端回应前不发送UDP的数据
            if (client_info_list[i].http_request_sent_len > udp.http_request_len) {
                udp_ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
                udp_ev.data.ptr = client_info_list + i;
                epoll_ctl(udp_efd, EPOLL_CTL_MOD, client_info_list[i].server_fd, &udp_ev);
            }
            return 0;
        }
    }

    return 1;
}

static void new_client()
{
    int i;

    for (i = 0; i < MAX_CLIENT_INFO; i++) {
        if (client_info_list[i].server_fd == -1) {
            if (recvClient(client_info_list + i) == 0)
                if (margeClient(client_info_list + i) != 0)
                    connectToServer(client_info_list + i);
            return;
        }
    }
}

static void http_udp_req_init()
{
    char dest[22];

    sprintf(dest, "%s:%u", inet_ntoa(udp.dst.sin_addr), ntohs(udp.dst.sin_port));
    if (udp.http_request) {
        udp.http_request_len = strlen(udp.http_request) + 2;
        udp.http_request = (char *)realloc(udp.http_request, udp.http_request_len + 1);
        if (udp.http_request == NULL)
            errors("httpudp http request initializate failed.");
        strcat(udp.http_request, "\r\n");
        udp.http_request = replace(udp.http_request, &udp.http_request_len, "[V]", 3, "HTTP/1.1", 8);
        udp.http_request = replace(udp.http_request, &udp.http_request_len, "[H]", 3, dest, strlen(dest));
        udp.http_request = replace(udp.http_request, &udp.http_request_len, "\\0", 2, "\0", 1);
        udp.http_request = replace(udp.http_request, &udp.http_request_len, "[M]", 3, "CONNECT", 7);
        udp.http_request = replace(udp.http_request, &udp.http_request_len, "[url]", 5, "/", 1);
        udp.http_request = replace(udp.http_request, &udp.http_request_len, "[U]", 3, "/", 1);
    } else {                    /* 默认使用CONNECT请求 */
        if (https.encodeCode) {
            dataEncode(dest, strlen(dest), https.encodeCode);
            udp.httpsProxy_encodeCode = https.encodeCode;
        }
        udp.http_request_len = default_ssl_request_len;
        copy_new_mem(default_ssl_request, default_ssl_request_len, &udp.http_request);
        udp.http_request = replace(udp.http_request, &udp.http_request_len, "[H]", 3, dest, strlen(dest));
        memcpy(&udp.dst, &https.dst, sizeof(udp.dst));
    }
    if (udp.http_request == NULL)
        errors("out of memory.");
    /* 保存原始生成的请求头，配合usr_hdr使用 */
    if (saveHdrs) {
        if (copy_new_mem(udp.http_request, udp.http_request_len, &udp.original_http_request) != 0)
            errors("out of memory.");
        udp.original_http_request_len = udp.http_request_len;
    }
}

void udp_init()
{
    int i;

    //初始化http请求
    http_udp_req_init();
    //初始化结构体
    memset(client_info_list, 0, sizeof(info_t) * MAX_CLIENT_INFO);
    for (i = 0; i < MAX_CLIENT_INFO; i++)
        client_info_list[i].server_fd = client_info_list[i].responseClientFd = -1;
    //创建epoll fd
    udp_efd = epoll_create(MAX_CLIENT_INFO * 2 + 1);
    if (udp_efd < 0) {
        perror("udp epoll_create()");
        exit(1);
    }
    //添加监听socket到epoll
    fcntl(global.udp_listen_fd, F_SETFL, O_NONBLOCK);
    udp_ev.data.fd = global.udp_listen_fd;
    udp_ev.events = EPOLLIN;
    epoll_ctl(udp_efd, EPOLL_CTL_ADD, global.udp_listen_fd, &udp_ev);
}

void *udp_loop(void *nullPtr)
{
    int n;

    while (1) {
        n = epoll_wait(udp_efd, udp_evs, MAX_CLIENT_INFO * 2 + 1, -1);
        while (n-- > 0) {
            if (udp_evs[n].data.fd == global.udp_listen_fd) {
                new_client();
            } else {
                if (udp_evs[n].events & EPOLLIN) {
                    recvServer((info_t *) udp_evs[n].data.ptr);
                }
                if (udp_evs[n].events & EPOLLOUT) {
                    outEvent((info_t *) udp_evs[n].data.ptr);
                }
            }
        }
    }

    return NULL;                //消除编译警告
}
