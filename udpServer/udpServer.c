/*
    httpUDPServer代理UDP过程:
        [接收客户端http请求并回应http请求]  这一步可有可无
        接收客户端数据   格式为: UDP目标地址[steuct in_addr](只有第一个包有) + UDP数据长度[uint16_t] + UDP真实数据
        发送UDP数据并接收UDP服务器回应的数据
        将UDP服务器回应的UDP数据返回给客户端，格式: UDP数据长度[uint16_t] + UDP真实数据
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <poll.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/netfilter_ipv4.h>

#define WEB_SOCKET_RSP "HTTP/1.1 101 Switching Protocols\r\nUpgrade:  websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ChameleonProxy httpUDP Server\r\nVia: ChameleonProxy httpUDP Server\r\n\r\n"
#define HTTP_RSP "HTTP/1.1 200 OK\r\nConnection: Keep-Alive\r\nContent-Length: 999999999\r\nServer: ChameleonProxy httpUDP Server\r\n\r\n"
#define SSL_RSP "HTTP/1.1 200 Connection established\r\nConnection: Keep-Alive\r\nServer: ChameleonProxy httpUDP Server\r\n\r\n"
#define BUFFER_SIZE 4096+65535
#define DEFAULT_TIMEOUT_S 20
#define DEFAULT_THEAD_POOL_SIZE 30
#define HTTP_TYPE 0
#define OTHER_TYPE 1

struct clientData {
    char buffer[BUFFER_SIZE+1], *client_data, *udpData;
    struct sockaddr_in udpDst;
    int client_data_len, clientfd, remote_udpfd;
    uint16_t udpData_len;
    unsigned sentRspHttpReq :1;
};

struct clientData publicConn;  //主线程设置该变量，子线程复制
pthread_cond_t thCond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t thMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t master_thId;  //主线程的线程id
int listenfd = -1,
    timeout_s = DEFAULT_TIMEOUT_S,
    thread_pool_size = DEFAULT_THEAD_POOL_SIZE;
uint8_t encodeCode = 0;

void usage()
{
    printf("httpudp server(0.3):\n"
    "Author: CuteBi\n"
    "E-mail: 915445800@qq.com\n"
    "    -l    \033[20G listen port\n"
    "    -t    \033[20G timeout(s)  defaule is %d s\n"
    "    -w   \033[20G worker proc\n"
    "    -p    \033[20G thread pool size default is %d\n"
    "    -e    \033[20G encode data code(128-255) default is 0\n"
    "    -u    \033[20G set uid\n\n", DEFAULT_TIMEOUT_S, DEFAULT_THEAD_POOL_SIZE);
    exit(0);
}

/* 对数据进行编码 */
void dataEncode(char *data, int data_len)
{
    while (data_len-- > 0)
        data[data_len] ^= encodeCode;
}

/* 判断请求类型 */
uint8_t request_type(char *req)
{
    if (strncmp(req, "GET", 3) == 0 || 
    strncmp(req, "POST", 4) == 0 ||
    strncmp(req, "CONNECT", 7) == 0 ||
    strncmp(req, "HEAD", 4) == 0 ||
    strncmp(req, "PUT", 3) == 0 ||
    strncmp(req, "OPTIONS", 7) == 0 ||
    strncmp(req, "MOVE", 4) == 0 ||
    strncmp(req, "COPY", 4) == 0 ||
    strncmp(req, "TRACE", 5) == 0 ||
    strncmp(req, "DELETE", 6) == 0 ||
    strncmp(req, "LINK", 4) == 0 ||
    strncmp(req, "UNLINK", 6) == 0 ||
    strncmp(req, "PATCH", 5) == 0 ||
    strncmp(req, "WRAPPED", 7) == 0)
        return HTTP_TYPE;
    else
        return OTHER_TYPE;
}

/* 回应HTTP请求 */
int rspHttpReq(struct clientData *client)
{
    /* 回应CONNECT请求 */
    if (strncmp(client->client_data, "CON", 3) == 0)
    {
        if (write(client->clientfd, SSL_RSP, sizeof(SSL_RSP) - 1) <= 0)
        {
            perror("ssl rsp write()");
            return 1;
        }
    }
    /* 回应WebSocket请求 */
    else if (strstr(client->client_data, "websocket"))
    {
        if (write(client->clientfd, WEB_SOCKET_RSP, sizeof(WEB_SOCKET_RSP) - 1) <= 0)
        {
            perror("websocket rsp write()");
            return 1;
        }
    }
    /* 回应HTTP请求 */
    else
    {
        if (write(client->clientfd, HTTP_RSP, sizeof(HTTP_RSP) - 1) <= 0)
        {
            perror("http rsp write()");
            return 1;
        }
    }

    client->sentRspHttpReq = 1;
    return 0;
}

/* 得到客户端数据中的udpDataLen dstAddr */
int parse_request(struct clientData *client)
{
    char *headerEnd;

    if (request_type(client->client_data) == OTHER_TYPE)
    {
        client->udpData = client->client_data;
    }
    else
    {
        headerEnd = strstr(client->client_data, "\n\r\n");
        if (headerEnd == NULL)
        {
            //puts("headerEnd NULL.");
            return 1;
        }
        *headerEnd = '\0';
        if (client->sentRspHttpReq == 0 && rspHttpReq(client) != 0)
            return 1;
        *headerEnd = '\n';
        client->udpData = headerEnd + 3;
    }
    if ((int)(client->client_data_len - (client->udpData - client->client_data) - sizeof(struct sockaddr_in) - sizeof(uint16_t)) <= 0)
        return 1;
    if (encodeCode)
        dataEncode(client->udpData, (int)(client->client_data_len - (client->udpData - client->client_data)));

    /* 复制UDP目标地址跟UDP长度 */
    memcpy(&client->udpDst, client->udpData, sizeof(struct sockaddr_in));
    memcpy(&(client->udpData_len), (uint16_t *)(client->udpData + sizeof(struct sockaddr_in)), sizeof(uint16_t));
    client->udpData += sizeof(struct sockaddr_in);
    //printf("len: [%u] dataLen: [%d], ip: [%s], port: [%d]\n", client->udpData_len, (int)(client->client_data_len - (client->udpData - client->client_data)), inet_ntoa(client->udpDst.sin_addr), ntohs(client->udpDst.sin_port));

    return 0;
}

int recvServer(struct clientData *server)
{
    int read_len;

    while ((read_len = recv(server->remote_udpfd, server->buffer + sizeof(uint16_t), BUFFER_SIZE - sizeof(uint16_t), MSG_DONTWAIT)) > 0)
    {
        //printf("%u: read remote: [%d]\n", pthread_self(), read_len);
        memcpy(server->buffer, &read_len, sizeof(uint16_t));
        //printf("server read_len: [%d], server->buffer: [%u]\n", read_len, *(uint16_t *)server->buffer);
        read_len += sizeof(uint16_t);
        if (encodeCode)
            dataEncode(server->buffer, read_len);
        if (write(server->clientfd, server->buffer, read_len) != read_len)
        {
            perror("write to client()");
            return 1;
        }
    }
    if (read_len == 0 || errno != EAGAIN)
    {
        perror("server recv()");
        return 1;
    }
    return 0;
}

/*
    发送客户端数据到服务器
    发送失败或者发送完成返回null
    未发送完成返回未发送的数据首地址
*/
char *sendServer(struct clientData *client)
{
    char *dataPtr;
    int write_len;

    dataPtr = client->client_data;
    //client->client_data_len > 1才能满意uint16_t这个类型的储存空间
    while (client->client_data_len > 1 && (int)(*(uint16_t *)dataPtr + sizeof(uint16_t)) <= client->client_data_len)
    {
        if ((write_len = write(client->remote_udpfd, dataPtr+sizeof(uint16_t), *(uint16_t *)dataPtr)) != *(uint16_t *)dataPtr)
        {
            perror("write to remote()");
            return NULL;
        }
        //printf("%u, fd: %d, write_len: %d, client_data_len: %d\n", pthread_self(), client->remote_udpfd, write_len, client->client_data_len);
        dataPtr += write_len + sizeof(uint16_t);
        client->client_data_len -= write_len + sizeof(uint16_t);
    }

    return client->client_data_len > 0 ? dataPtr : NULL;
}

int recvClient(struct clientData *client)
{
    char *new_data, *dataPtr;
    int read_len;

    do {
        new_data = (char *)realloc(client->client_data, client->client_data_len + BUFFER_SIZE);
        if (new_data == NULL)
            return 1;
        client->client_data = new_data;
        read_len = recv(client->clientfd, client->client_data + client->client_data_len, BUFFER_SIZE, MSG_DONTWAIT);
        printf("%lu: get client len: [%d]\n", pthread_self(), read_len);
        if (read_len <= 0)
        {
            if (read_len == 0 || errno != EAGAIN)
            {
                perror("client read()");
                return 1;
            }
            return 0;
        }
        if (encodeCode)
            dataEncode(client->client_data + client->client_data_len, read_len);
        client->client_data_len += read_len;
        dataPtr = sendServer(client);
        //write()发生错误
        if (dataPtr == NULL && client->client_data_len > 0)
        {
            return 1;
        }
        else if (client->client_data_len > 0)
        {
            memmove(client->client_data, dataPtr, client->client_data_len);
        }
        else
        {
            free(client->client_data);
            client->client_data = NULL;
            client->client_data_len = 0;
        }
    } while (read_len == BUFFER_SIZE);

    return 0;
}

void forwardData(struct clientData *client)
{
    struct pollfd pfds[2];

    pfds[0].fd = client->remote_udpfd;
    pfds[1].fd = client->clientfd;
    pfds[0].events = pfds[1].events = POLLIN;
    while (poll(pfds, 2, timeout_s*1000) > 0)
    {
        printf("a event %lu\n", pthread_self());
        if (pfds[0].revents & POLLIN)
        {
            printf("recvServer %lu\n", pthread_self());
            if (recvServer(client) != 0)
                return;
        }
        if (pfds[1].revents & POLLIN)
        {
            printf("recvServer %lu\n", pthread_self());
            if (recvClient(client) != 0)
                return;
        }
    }
}

int sendFirstData(struct clientData *client)
{
    char *dataPtr;

    printf("%lu: sendFirstData\n", pthread_self());
    client->remote_udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (client->remote_udpfd < 0)
    {
        perror("socket()");
        return 1;
    }
    connect(client->remote_udpfd, (struct sockaddr *)&client->udpDst, sizeof(struct sockaddr_in));
    client->client_data = client->udpData;
    client->client_data_len = client->udpData_len + sizeof(uint16_t);
    dataPtr = sendServer(client);
    if (dataPtr == NULL)
    {
        if (client->client_data_len > 0)
            return 1;
        client->client_data = NULL;
    }
    else
    {
        client->client_data = (char *)malloc(client->client_data_len);
        if (client->client_data == NULL)
            return 1;
        memcpy(client->client_data, dataPtr, client->client_data_len);
    }

    printf("%lu: sendFirstData end\n", pthread_self());
    return 0;
}

int parseClient(struct clientData *client)
{
    int read_len, count;

    printf("%lu: parseClient\n", pthread_self());
    count = 0;
    client->client_data = client->buffer;
    do {
        //printf("%u: start read\n", pthread_self());
        read_len = read(client->clientfd, client->client_data + client->client_data_len, BUFFER_SIZE - client->client_data_len);
        //printf("%u: read_len = %d\n", pthread_self(), read_len);
        if (read_len <= 0)
        {
            perror("parseClient read()");
            return 1;
        }
        client->client_data_len += read_len;
        client->client_data[client->client_data_len]  = '\0';
        count++;
    } while (parse_request(client) != 0 && count < 5);

    //printf("%u: parseClient end\n", pthread_self());
    return count == 5 ? 1 : 0;
}

void *new_connection(void *nullPtr)
{
    #define NO_COPY_SIZE (BUFFER_SIZE + 1 + (sizeof(char *)<<1) + sizeof(struct sockaddr_in))  //struct clientData不需要全部复制
    struct clientData client;

    //printf("new_connection: %u\n", pthread_self())
    memcpy((void *)(&client) + NO_COPY_SIZE, (void *)(&publicConn) + NO_COPY_SIZE, sizeof(struct clientData) - NO_COPY_SIZE);
    pthread_kill(master_thId, SIGUSR1);
    /* 读取客户端数据 */
    if (parseClient(&client) == 0 && sendFirstData(&client) == 0)
        forwardData(&client);
    else
        puts("parseClient() client error");

    close(client.remote_udpfd);
    close(client.clientfd);
    if (client.client_data != client.buffer && client.client_data != client.udpData)
        free(client.client_data);

    //printf("new_connection end: %u\n", pthread_self());
    return NULL;
}

int accept_client()
{
    struct sockaddr_in addr;
    struct timeval tv = {timeout_s, 0};
    socklen_t addr_len = sizeof(addr);

    publicConn.clientfd = accept(listenfd, (struct sockaddr *)&addr, &addr_len);
    if (publicConn.clientfd < 0)
    {
        perror("accept()");
        return 1;
    }
    setsockopt(publicConn.clientfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    return 0;
}

void *threadPool_waitTask(void *ptr)
{
    int *isBusy;

    isBusy = (int *)ptr;
    while (1)
    {
        pthread_cond_wait(&thCond, &thMutex);
        pthread_mutex_unlock(&thMutex);  //解锁，让其他线程可以并发
        *isBusy = 1;
        new_connection(NULL);
        *isBusy = 0;
    }

    return NULL;
}

void loop()
{
    pthread_t th_id;
    pthread_attr_t attr;
    sigset_t sig;
    int *th_isBusy,  //线程执行繁忙值为1，空闲值为0
        i, signum;

    //初始化publicConn
    memset(&publicConn, 0, sizeof(struct clientData));
    publicConn.remote_udpfd = -1;
    /* 创建线程池 */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    th_isBusy = (int *)calloc(thread_pool_size, sizeof(int));
    if (th_isBusy == NULL)
    {
        perror("calloc()");
        return;
    }
    for (i = 0; i < thread_pool_size; i++)
        pthread_create(&th_id, &attr, &threadPool_waitTask, (void *)(th_isBusy + i));
    /* 初始化信号设置，用于子线程告诉主线程内存已经拷贝完毕 */
    sigemptyset(&sig);
    sigaddset(&sig, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &sig, NULL);
    master_thId = pthread_self();
    while (1)
    {
        if (accept_client() != 0)
        {
            sleep(3);
            continue;
        }
        /* 如果线程池有空闲线程则调用空闲线程处理 */
        for (i = 0; i < thread_pool_size; i++)
        {
            if (th_isBusy[i] == 0)
            {
                pthread_cond_signal(&thCond);
                break;
            }
        }
        /* 线程池没有空闲线线程，创建新线程运行任务 */
        if (i == thread_pool_size)
        {
            if (pthread_create(&th_id, &attr, &new_connection, NULL) != 0)
            {
                close(publicConn.clientfd);
                continue;
            }
        }
        sigwait(&sig, &signum);
    }
}

int create_listen(char *ip, int port)
{
    int fd, optval = 1;
    struct sockaddr_in addr;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket()");
        return -1;
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
    {
        close(fd);
        perror("setsockopt()");
        return -1;
    }
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        close(fd);
        perror("bind()");
        return -1;
    }
    if (listen(fd, 500) != 0)
    {
        close(fd);
        perror("listen()");
        return -1;
    }

    return fd;
}

void readCmd(int argc, char **argv)
{
    int opt, worker_proc;

    while ((opt = getopt(argc, argv, "l:u:e:w:p:t:h")) != -1)
    {
        switch (opt)
        {
            case 't':
                timeout_s = atoi(optarg);
            break;

            case 'e':
                encodeCode = atoi(optarg);
            break;

            case 'l':
                    listenfd = create_listen((char *)"0.0.0.0", atoi(optarg));
            break;

            case 'u':
                if (setgid(atoi(optarg)) || setuid(atoi(optarg)))
                    perror("setgid(or setuid)()");
            break;

            case 'w':
                worker_proc = atoi(optarg);
                while (worker_proc-- > 1 && fork());
            break;

            case 'p':
                thread_pool_size = atoi(optarg);
            break;

            case 'h':
                usage();
            break;
        }
    }

}
int main(int argc, char **argv)
{
    readCmd(argc, argv);
    if (listenfd < 0)
    {
        usage();
        return 1;
    }
    if (daemon(1, 1) == -1)
    {
        perror("daemon()");
        return 1;
    }
    signal(SIGPIPE, SIG_IGN);
    loop();

    return 0;
}
