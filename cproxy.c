#include "cproxy.h"

ssize_t readLine(int fd, void *buffer, size_t n)
{
    ssize_t numRead;
    size_t totRead;
    char *buf;
    char ch;

    if (n <= 0 || buffer == NULL) {
        errno = EINVAL;
        return -1;
    }

    buf = buffer;

    totRead = 0;
    for (;;) {
        numRead = receive_data(fd, &ch, 1);

        if (numRead == -1) {
            if (errno == EINTR)
                continue;
            else
                return -1;      // 未知错误

        } else if (numRead == 0) { // EOF
            if (totRead == 0)   // No bytes read; return 0
                return 0;
            else                // Some bytes read; add '\0'
                break;

        } else {

            if (totRead < n - 1) { // Discard > (n - 1) bytes
                totRead++;
                *buf++ = ch;
            }

            if (ch == '\n')
                break;
        }
    }

    *buf = '\0';
    return totRead;
}

// 读取header
int read_header(int fd, void *buffer)
{
    memset(header_buffer, 0, MAX_HEADER_SIZE);
    char line_buffer[2048];
    char *base_ptr = header_buffer;

    for (;;) {
        memset(line_buffer, 0, 2048);

        int total_read = readLine(fd, line_buffer, 2048);
        if (total_read <= 0) {
            return -1;
        }
        // 防止header缓冲区蛮越界
        if (base_ptr + total_read - header_buffer <= MAX_HEADER_SIZE) {
            strncpy(base_ptr, line_buffer, total_read);
            base_ptr += total_read;
        } else {
            return -1;
        }

        // 读到了空行，http头结束
        if (strcmp(line_buffer, "\r\n") == 0 || strcmp(line_buffer, "\n") == 0) {
            break;
        }

    }
    return 0;

}

// 提取主机、端口
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

// 响应隧道连接请求
int send_tunnel_ok(int client_sock)
{
    char *resp = "HTTP/1.1 200 Connection Established\r\n\r\n"; // 把字符串写入client_sock流
    int len = strlen(resp);
    char buffer[len + 1];
    strcpy(buffer, resp);
    if (send_data(client_sock, buffer, len) < 0) {
        return -1;
    }
    return 0;
}

// 转发数据,读取source_sock流发送到destination_sock流
void forward_data(int source_sock, int destination_sock)
{
    char buffer[BUF_SIZE];
    int n;
    while ((n = receive_data(source_sock, buffer, BUF_SIZE)) > 0) {
        send_data(destination_sock, buffer, n);
    }
    shutdown(destination_sock, SHUT_RDWR);
    shutdown(source_sock, SHUT_RDWR);
}

// 转发头字符串到destination_sock
void forward_header(int destination_sock)
{
    rewrite_header();
    int len = strlen(header_buffer);
    send_data(destination_sock, header_buffer, len);
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

}

// 接收数据
int receive_data(int socket, char *buffer, int len)
{
    int n = recv(socket, buffer, len, 0);
    return n;
}

// 发送数据
int send_data(int socket, char *buffer, int len)
{
    return send(socket, buffer, len, 0);
}

// 创建连接
int create_connection()
{
    struct sockaddr_in server_addr;
    struct hostent *server;
    int sock;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }
    if ((server = gethostbyname(remote_host)) == NULL) {
        errno = EFAULT;
        return -1;
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(remote_port);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        return -1;
    }

    return sock;
}

// 处理客户端的连接
void handle_client(int client_sock, struct sockaddr_in client_addr, conf *m)
{
    int HTTPS = 0;
    read_header(client_sock, header_buffer);
    extract_host(header_buffer);
    replacement_http_head(header_buffer, remote_host, &remote_port, &HTTPS, m);

    printf("%s\n", remote_host);
    printf("%d\n", remote_port);
    printf("%d\n", HTTPS);
    printf("%s", header_buffer);

    if ((remote_sock = create_connection()) < 0) { // 先创建连接
        return;
    }

    if (fork() == 0) {          // 创建子进程用于从客户端转发数据到远端socket接口
        if (HTTPS == 0) {
            forward_header(remote_sock); //普通的http请求先转发header
            forward_data(client_sock, remote_sock);
        } else {
            char buffer[BUF_SIZE];
            int n;
            while ((n = read(client_sock, buffer, BUF_SIZE)) > 0) {
                write(remote_sock, buffer, n);
            }
        }
        if (HTTPS == 1) {
			forward_header(remote_sock); //普通的http请求先转发header
            forward_data(client_sock, remote_sock);
		} else {
			char buffer[BUF_SIZE];
            int n;
            while ((n = read(client_sock, buffer, BUF_SIZE)) > 0) {
                write(remote_sock, buffer, n);
            }
		}
        exit(0);
    }
    if (fork() == 0) {          // 创建子进程用于转发从远端socket接口过来的数据到客户端
        if (HTTPS == 1) {
            send_tunnel_ok(client_sock);
        }
        forward_data(remote_sock, client_sock);
        exit(0);
    }

    close(remote_sock);
    close(client_sock);
}

// 守护
int init_daemon(int nochdir, int noclose, conf * p)
{
    FILE *fp = fopen(p->server_pid_file, "w");
    int pid;

    if ((pid = fork()) < 0) {
        return -1;
    } else if (0 != pid) {
        exit(0);
    }
    //child 1 continues...   

    //become session leader   
    if (setsid() < 0) {
        return -1;
    }

    signal(SIGHUP, SIG_IGN);
    if ((pid = fork()) < 0) {
        return -1;
    } else if (0 != pid) {
        fprintf(fp, "%d", pid);
        exit(0);
    }
    //child 2 continues...   

    //change working directory   
    if (0 == nochdir) {
        chdir("/");
    }
    //close off file descriptors
    /*
       int i;
       for (i = 0; i < 64; i++) {
       close(i);
       }
     */

    //redirect stdin,stdout,stderror to "/dev/null"   
    if (0 == noclose) {
        open("/dev/null", O_RDONLY);
        open("/dev/null", O_RDWR);
        open("/dev/null", O_RDWR);
    }
    fclose(fp);
    return 0;
}

// 判断pid文件存放目录
void log_dir(conf *p)
{
    char *d = strchr(p->server_pid_file, '/');
    if (d != NULL) {
        int l_d = p->len_server_pid_file - strlen(d);
        char pid_dir[l_d];
        strncpy(pid_dir, p->server_pid_file, l_d - 1);
        printf("%d\n", l_d);
        printf("%s\n", pid_dir);

        if (access(pid_dir, F_OK) != 0) {
            printf("Pid File Can Not Open To Write\n");
            errno = 0;
            mkdir("log", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
            perror("mkdir");
        }
    }
}

void sigchld_handler(int signal)
{
    while (waitpid(-1, NULL, WNOHANG) > 0) ;
}

// 主函数
int _main(int argc, char *argv[])
{
    char *inifile = "conf/cproxy.ini";
    conf *m = (struct CONF *)malloc(sizeof(struct CONF));
    read_conf(inifile, m);
    int PORT = m->server_port;
/*
    printf("%d\n", m->server_port);
    printf("%s\n", m->server_pid_file);
    printf("%s\n", m->http_ip);
    printf("%d\n", m->http_port);
    printf("%s\n", m->http_del);
    printf("%s\n", m->http_first);
    printf("%s\n", m->https_ip);
    printf("%d\n", m->https_port);
    printf("%s\n", m->https_del);
    printf("%s\n", m->https_first);
*/

    int opt;
    char optstrs[] = ":l:d";
    while (-1 != (opt = getopt(argc, argv, optstrs))) {
        switch (opt) {
        case 'l':
            PORT = atoi(optarg);
            break;
        case 'd':{
                init_daemon(1, 1, m);
            }
            break;
        default:
            ;
        }
    }

    header_buffer = (char *)malloc(MAX_HEADER_SIZE);

    signal(SIGCHLD, sigchld_handler); // 防止子进程变成僵尸进程

    int server_sock, optval;
    struct sockaddr_in server_addr;
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }
    if (setsockopt
        (server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        return -1;
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr))
        != 0) {
        return -1;
    }
    if (listen(server_sock, 17) < 0) {
        return -1;
    }

    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size = sizeof(clnt_addr);
    while (1) {
        int client_socket = accept(server_sock, (struct sockaddr *)&clnt_addr,
                                   &clnt_addr_size);

        if (fork() == 0) {      // 创建子进程处理客户端连接请求
            close(server_sock);
            handle_client(client_socket, clnt_addr, m);
            exit(0);
        }
        close(client_socket);
    }
    free(header_buffer);        // 收回内存
    free_conf(m);
}

int main(int argc, char *argv[])
{
    return _main(argc, argv);
}
