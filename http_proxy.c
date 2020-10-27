#include "http_proxy.h"
#include "main.h"

int sslEncodeCode;
int remote_port;
char remote_host[128];

/* 对数据进行编码 */
void dataEncode(char *data, int data_len)
{
    if (sslEncodeCode)
        while (data_len-- > 0)
            data[data_len] ^= sslEncodeCode;
}

static char *read_data(conn * in, char *data, int *data_len)
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

void close_connection(conn * conn)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, conn->fd, NULL);
    close(conn->fd);
    if ((conn - cts) & 1) {
        char *server_data;
        server_data = conn->header_buffer;
        memset(conn, 0, sizeof(*conn));
        conn->header_buffer = server_data;
        conn--->fd = -1;
    } else {
        free(conn->header_buffer);
        memset(conn, 0, sizeof(*conn));
        conn++->fd = -1;
    }

    if (conn->fd >= 0)
        close_connection(conn);
}

static void serverToClient(conn * server)
{
    int write_len;
    conn *client;
    client = server - 1;

    while ((server->header_buffer_len = read(server->fd, server->header_buffer, BUFFER_SIZE)) > 0) {
        dataEncode(server->header_buffer, server->header_buffer_len);
        write_len = write(client->fd, server->header_buffer, server->header_buffer_len);
        if (write_len <= 0) {
            if (write_len == 0 || errno != EAGAIN)
                close_connection(server);
            else
                server->sent_len = 0;
            return;
        } else if (write_len < server->header_buffer_len) {
            server->sent_len = write_len;
            ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
            ev.data.ptr = client;
            epoll_ctl(epollfd, EPOLL_CTL_MOD, client->fd, &ev);
            return;
        }
        if (server->header_buffer_len < BUFFER_SIZE)
            break;
    }
    if (server->header_buffer_len == 0 || (server->header_buffer_len == -1 && errno != EAGAIN))
        close_connection(server);
    else
        server->header_buffer_len = server->sent_len = 0;

}


void clientToserver(conn * in)
{
    int write_len;
    conn *remote;
    remote = in + 1;

    write_len = write(remote->fd, in->header_buffer, in->header_buffer_len);
    if (write_len == in->header_buffer_len) {
        in->header_buffer_len = 0;
        in->header_buffer = NULL;
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

void tcp_in(conn * in, conf * configure)
{
    conn *remote;
    
    if (in->fd < 0)
        return;
    // 如果in - cts是奇数,那么是服务端触发事件
    if ((in - cts) & 1) {
        in->timer = (in - 1)->timer = 0;
        serverToClient(in);
        return;
    }
    
    in->timer = (in + 1)->timer = 0;
    in->header_buffer = read_data(in, in->header_buffer, &in->header_buffer_len);
    if (in->header_buffer == NULL) {
        close_connection(in);
        return;
    }
    remote = in + 1;
    if (in->request_type == OTHER_TYPE)
    {
        goto handle_data_complete;
    }
    
    if (request_type(in->header_buffer) == HTTP_TYPE) {
        in->header_buffer = request_head(in, configure);
        struct epoll_event epollEvent;
        remote->fd = create_connection6(remote_host, remote_port);
        epollEvent.events = EPOLLIN | EPOLLOUT | EPOLLET;
        epollEvent.data.ptr = remote;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, remote->fd, &epollEvent);
    }
    //printf("%s", in->header_buffer);
    dataEncode(in->header_buffer, in->header_buffer_len);
    
    handle_data_complete:
    if (remote->fd >= 0) {
        clientToserver(in);
        //tcp_out(remote);
    }

    return;
}

void tcp_out(conn * out)
{
    conn *from;
    int write_len;

    if (out->fd == -1)
        return;
    else if ((out - cts) & 1)
        from = out - 1;
    else
        from = out + 1;
    from->timer = out->timer = 0;
    write_len = write(out->fd, from->header_buffer + from->sent_len, from->header_buffer_len - from->sent_len);
    if (write_len == from->header_buffer_len - from->sent_len) {
        // 服务端的数据可能没全部写入到客户端
        if ((from - cts) & 1) {
            serverToClient(from);
            if (from->fd >= 0 && from->header_buffer == 0) {
                ev.events = EPOLLIN | EPOLLET;
                ev.data.ptr = out;
                epoll_ctl(epollfd, EPOLL_CTL_MOD, out->fd, &ev);
            }
        } else {
            ev.events = EPOLLIN | EPOLLET;
            ev.data.ptr = out;
            epoll_ctl(epollfd, EPOLL_CTL_MOD, out->fd, &ev);
            free(from->header_buffer);
            from->header_buffer = NULL;
            from->header_buffer_len = 0;
        }
    } else if (write_len > 0) {
        from->sent_len += write_len;
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.data.ptr = out;
        epoll_ctl(epollfd, EPOLL_CTL_MOD, out->fd, &ev);
    } else if (errno != EAGAIN) {
        close_connection(out);
    }
    return;
}
