#include "http_proxy.h"
#include "main.h"

int sslEncodeCode;
int remote_port;
char remote_host[CACHE_SIZE];

/* 对数据进行编码 */
void dataEncode(char *data, int data_len)
{
    if (sslEncodeCode)
        while (data_len-- > 0)
            data[data_len] ^= sslEncodeCode;
}

static char *read_data(conn_t * in, char *data, int *data_len)
{
    char *new_data;
    int read_len;

    do {
        new_data = (char *)realloc(data, *data_len + BUFFER_SIZE + 1);
        if (new_data == NULL) {
            free(data);
            return NULL;
        }
        data = new_data;
        read_len = read(in->fd, data + *data_len, BUFFER_SIZE);
        // 判断是否关闭连接
        if (read_len <= 0) {
            if (read_len == 0 || *data_len == 0 || errno != EAGAIN) {
                free(data);
                return NULL;
            }
            break;
        }
        *data_len += read_len;
    } while (read_len == BUFFER_SIZE);
    *(data + *data_len) = '\0';

    return data;
}

void close_connection(conn_t * conn)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, conn->fd, NULL);
    close(conn->fd);
    if ((conn - cts) & 1) {
        char *server_data;

        server_data = conn->ready_data;
        memset(conn, 0, sizeof(conn_t));
        conn->ready_data = server_data;
        conn--->fd = -1;
    } else {
        free(conn->ready_data);
        free(conn->incomplete_data);
        memset(conn, 0, sizeof(conn_t));
        conn++->fd = -1;
    }
    if (conn->fd >= 0)
        close_connection(conn);
}

static void serverToClient(conn_t * server)
{
    conn_t *client;
    int write_len;

    client = server - 1;
    while ((server->ready_data_len = read(server->fd, server->ready_data, BUFFER_SIZE)) > 0) {
        dataEncode(server->ready_data, server->ready_data_len);
        write_len = write(client->fd, server->ready_data, server->ready_data_len);
        if (write_len <= 0) {
            if (write_len == 0 || errno != EAGAIN)
                close_connection(server);
            else
                server->sent_len = 0;
            return;
        } else if (write_len < server->ready_data_len) {
            server->sent_len = write_len;
            ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
            ev.data.ptr = client;
            epoll_ctl(epollfd, EPOLL_CTL_MOD, client->fd, &ev);
            return;
        }
        if (server->ready_data_len < BUFFER_SIZE)
            break;
    }
    //判断是否关闭连接
    if (server->ready_data_len == 0 || (server->ready_data_len == -1 && errno != EAGAIN))
        close_connection(server);
    else
        server->ready_data_len = server->sent_len = 0;
}

void clientToserver(conn_t * in)
{
    int write_len;
    conn_t *remote;
    remote = in + 1;

    write_len = write(remote->fd, in->ready_data, in->ready_data_len);
    if (write_len == in->ready_data_len) {
        in->ready_data_len = 0;
        in->ready_data = NULL;
    } else {
        close_connection(remote);
    }

    return;

}

// 判断请求类型
static int8_t request_type(char *data)
{
    if (strncmp(data, "GET", 3) == 0 || 
        strncmp(data, "POST", 4) == 0 || 
        strncmp(data, "CONNECT", 7) == 0 || 
        strncmp(data, "HEAD", 4) == 0 || 
        strncmp(data, "PUT", 3) == 0 || 
        strncmp(data, "OPTIONS", 7) == 0 || 
        strncmp(data, "MOVE", 4) == 0 || 
        strncmp(data, "COPY", 4) == 0 || 
        strncmp(data, "TRACE", 5) == 0 || 
        strncmp(data, "DELETE", 6) == 0 || 
        strncmp(data, "LINK", 4) == 0 || 
        strncmp(data, "UNLINK", 6) == 0 || 
        strncmp(data, "PATCH", 5) == 0 || 
        strncmp(data, "WRAPPED", 7) == 0)
        return HTTP_TYPE;
    return OTHER_TYPE;
}

// Check for valid IPv4 or Iv6 string. Returns AF_INET for IPv4, AF_INET6 for IPv6
int check_ipversion(char *address)
{
    struct in6_addr bindaddr;

    if (inet_pton(AF_INET, address, &bindaddr) == 1) {
        return AF_INET;
    } else {
        if (inet_pton(AF_INET6, address, &bindaddr) == 1) {
            return AF_INET6;
        }
    }
    return 0;
}

int create_connection6(char *remote_host, int remote_port)
{
    struct addrinfo hints, *res = NULL;
    int sock;
    int validfamily = 0;
    char portstr[CACHE_SIZE];

    memset(&hints, 0x00, sizeof(hints));

    hints.ai_flags = AI_NUMERICSERV; // numeric service number, not resolve
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    sprintf(portstr, "%d", remote_port);

    // check for numeric IP to specify IPv6 or IPv4 socket
    if ((validfamily = check_ipversion(remote_host)) != 0) {
        hints.ai_family = validfamily;
        hints.ai_flags |= AI_NUMERICHOST; // remote_host是有效的数字ip，跳过解析
    }
    // 检查指定的主机是否有效。 如果remote_host是主机名，尝试解析地址
    if (getaddrinfo(remote_host, portstr, &hints, &res) != 0) {
        errno = EFAULT;
        perror("getaddrinfo");
        return -1;
    }

    if ((sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
        perror("socket");
        return -1;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        return -1;
    }

    if (res != NULL)
        freeaddrinfo(res);

    return sock;
}

/* 读取到的数据全部就绪，将incomplete_data复制到ready_data */
static int8_t copy_data(conn_t * ct)
{
    dataEncode(ct->incomplete_data, ct->incomplete_data_len);
    if (ct->ready_data) {
        char *new_data;

        new_data = (char *)realloc(ct->ready_data, ct->ready_data_len + ct->incomplete_data_len);
        if (new_data == NULL)
            return 1;
        ct->ready_data = new_data;
        memcpy(new_data + ct->ready_data_len, ct->incomplete_data, ct->incomplete_data_len);
        ct->ready_data_len += ct->incomplete_data_len;
        free(ct->incomplete_data);
    } else {
        ct->ready_data = ct->incomplete_data;
        ct->ready_data_len = ct->incomplete_data_len;
    }

    ct->incomplete_data = NULL;
    ct->incomplete_data_len = 0;

    return 0;
}

void tcp_in(conn_t * in, conf * configure)
{
    conn_t *server;

    if (in->fd < 0)
        return;
    //如果in - cts是奇数，那么是服务端触发事件
    if ((in - cts) & 1) {
        in->timer = (in - 1)->timer = 0;
        if (in->ready_data_len <= 0)
            serverToClient(in);
        return;
    }

    in->timer = (in + 1)->timer = 0;
    in->incomplete_data = read_data(in, in->incomplete_data, &in->incomplete_data_len);

    //printf("%s", in->incomplete_data);
    if (in->incomplete_data == NULL) {
        close_connection(in);
        return;
    }
    server = in + 1;
    server->request_type = in->request_type = request_type(in->incomplete_data);

    if (request_type(in->incomplete_data) == HTTP_TYPE) {
        in->incomplete_data = request_head(in, configure);
        server->fd = create_connection6(remote_host, remote_port);
        if (server->fd < 0)
            printf("remote->fd ERROR!\n");
        fcntl(server->fd, F_SETFL, O_NONBLOCK);
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.data.ptr = server;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, server->fd, &ev);
    }

    if (in->incomplete_data == NULL || copy_data(in) != 0) {
        close_connection(in);
        return;
    }
    // 这个判断是防止 多次读取客户端数据，但是没有和服务端建立连接，导致报错
    if (server->fd >= 0)
        tcp_out(server);
}

void tcp_out(conn_t * to)
{
    conn_t *from;
    int write_len;

    if (to->fd == -1)
        return;
    else if ((to - cts) & 1)
        from = to - 1;
    else
        from = to + 1;
    from->timer = to->timer = 0;
    write_len = write(to->fd, from->ready_data + from->sent_len, from->ready_data_len - from->sent_len);
    if (write_len == from->ready_data_len - from->sent_len) {
        //服务端的数据可能没全部写入到客户端
        if ((from - cts) & 1) {
            serverToClient(from);
            if (from->fd >= 0 && from->ready_data_len == 0) {
                ev.events = EPOLLIN | EPOLLET;
                ev.data.ptr = to;
                epoll_ctl(epollfd, EPOLL_CTL_MOD, to->fd, &ev);
            }
        } else {
            ev.events = EPOLLIN | EPOLLET;
            ev.data.ptr = to;
            epoll_ctl(epollfd, EPOLL_CTL_MOD, to->fd, &ev);
            free(from->ready_data);
            from->ready_data = NULL;
            from->ready_data_len = 0;
        }
    } else if (write_len > 0) {
        from->sent_len += write_len;
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.data.ptr = to;
        epoll_ctl(epollfd, EPOLL_CTL_MOD, to->fd, &ev);
    } else if (errno != EAGAIN) {
        close_connection(to);
    }
}
