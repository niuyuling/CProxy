#include "cproxy.h"
#include "kill.h"

char *read_data(int client_sock, char *data, int *data_len)
{
    char *new_data;
    int read_len;

    do {
        new_data = (char *)realloc(data, *data_len + BUF_SIZE + 1);
        if (new_data == NULL) {
            free(data);
            return NULL;
        }
        data = new_data;
        read_len = read(client_sock, data + *data_len, BUF_SIZE);
        if (read_len <= 0) {
            if (read_len == 0 || *data_len == 0 || errno != EAGAIN) {
                free(data);
                return NULL;
            }
            break;
        }
        *data_len += read_len;
    } while (read_len == BUF_SIZE);
    *(data + *data_len) = '\0';

    return data;
}

void servertoclient(int remote_sock, int client_sock, char *complete_data,
                    int *len_complete_data)
{
    while ((*len_complete_data =
            read(remote_sock, complete_data, BUF_SIZE)) > 0) {
        write(client_sock, complete_data, *len_complete_data);
    }
    return;
}

void clienttoserver(int remote_sock, char *complete_data,
                    int *len_complete_data)
{
    write(remote_sock, complete_data, *len_complete_data);
    complete_data = NULL;
    complete_data = 0;
    return;
}

// 处理客户端的连接
void handle_client(int client_sock, struct sockaddr_in client_addr,
                   conf * configure)
{
    read_data(client_sock, header_buffer, &len_header_buffer); // 第一次读取客户端(浏览器)数据
    SIGN = request_type(header_buffer); // 获取请求消息类型
    extract_host(header_buffer); // 提取真实Host
    replacement_http_head(header_buffer, remote_host, &remote_port, &SIGN,
                          configure);

    //printf("%s", header_buffer);

    if ((remote_sock = create_connection(configure, SIGN)) < 0) {
        return;
    }

    if (fork() == 0) {
        if (SIGN == HTTP_CONNECT) {
            clienttoserver(remote_sock, header_buffer, &len_header_buffer);
            forward_data(client_sock, remote_sock);
        } else if (SIGN == HTTP_OTHERS || SIGN == HTTP) {
            forward_header(remote_sock); //普通的http请求先转发header
            forward_data(client_sock, remote_sock);
        }
        _exit(0);
    }

    if (fork() == 0) {
        if (SIGN == HTTP_CONNECT) {
            //servertoclient(remote_sock, client_sock, complete_data, &len_complete_data);
            forward_data(remote_sock, client_sock);
        } else if (SIGN == HTTP_OTHERS || SIGN == HTTP) {
            forward_data(remote_sock, client_sock);
        }
        _exit(0);
    }

    close(client_sock);
    close(remote_sock);
    return;
}

int send_data(int socket, char *buffer, int len)
{
    return send(socket, buffer, len, 0);
}

int receive_data(int socket, char *buffer, int len)
{
    int n = recv(socket, buffer, len, 0);
    return n;
}

void forward_data(int source_sock, int destination_sock)
{
    char buffer[BUF_SIZE];
    int n;
    while ((n = receive_data(source_sock, buffer, BUF_SIZE)) > 0) {
        send_data(destination_sock, buffer, n);
    }
    shutdown(destination_sock, SHUT_RDWR);
    shutdown(source_sock, SHUT_RDWR);
    return;
}

int create_connection(conf * configure, int SIGN)
{
    struct sockaddr_in server_addr;
    struct hostent *server = NULL;
    int sock;
    int optval;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return CLIENT_SOCKET_ERROR;
    }

    if (SIGN == HTTP_CONNECT) {
        if ((server = gethostbyname(configure->https_ip)) == NULL) {
            errno = EFAULT;
            return CLIENT_RESOLVE_ERROR;
        }
    } else if (SIGN == HTTP_OTHERS || SIGN == HTTP) {
        if ((server = gethostbyname(configure->http_ip)) == NULL) {
            errno = EFAULT;
            return CLIENT_RESOLVE_ERROR;
        }
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (SIGN == HTTP_CONNECT) {
        server_addr.sin_port = htons(configure->https_port);
    } else if (SIGN == HTTP_OTHERS || SIGN == HTTP) {
        server_addr.sin_port = htons(configure->http_port);
    }

    struct linger so_linger;
    so_linger.l_onoff = 1;
    so_linger.l_linger = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_LINGER, &so_linger, sizeof so_linger) <
        0) {
        return SERVER_SETSOCKOPT_ERROR;
    }

    if (setsockopt
        (server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        return SERVER_SETSOCKOPT_ERROR;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        return CLIENT_CONNECT_ERROR;
    }

    return sock;
}

int create_server_socket(int port)
{
    int server_sock;
    int optval;
    struct sockaddr_in server_addr;

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return SERVER_SOCKET_ERROR;
    }

    struct linger so_linger;
    so_linger.l_onoff = 1;
    so_linger.l_linger = 0;
    if (setsockopt
        (server_sock, SOL_SOCKET, SO_LINGER, &so_linger,
         sizeof so_linger) < 0) {
        return SERVER_SETSOCKOPT_ERROR;
    }

    if (setsockopt
        (server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        return SERVER_SETSOCKOPT_ERROR;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr))
        != 0) {
        return SERVER_BIND_ERROR;
    }

    if (listen(server_sock, 20) < 0) {
        return SERVER_LISTEN_ERROR;
    }

    return server_sock;
}

// 守护
int init_daemon(int nochdir, int noclose, conf * configure, char *path)
{
    char *p = strcat(path, configure->server_pid_file);
    FILE *fp = fopen(p, "w");
    if (fp == NULL) {
        fclose(fp);
        printf("%s Open Failed\n", p);
        exit(1);
    }

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
    //redirect stdin,stdout,stderror to "/dev/null"
    if (0 == noclose) {
        open("/dev/null", O_RDONLY);
        open("/dev/null", O_RDWR);
        open("/dev/null", O_RDWR);
    }
    fclose(fp);
    return 0;
}

// 处理僵尸进程
void sigchld_handler(int signal)
{
    while (waitpid(-1, NULL, WNOHANG) > 0) ;
    return;
}

void server_loop(conf * configure)
{
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);

    while (1) {
        client_sock =
            accept(server_sock, (struct sockaddr *)&client_addr, &addrlen);

        if (fork() == 0) {      // 创建子进程处理客户端连接请求
            close(server_sock);
            handle_client(client_sock, client_addr, configure);
            exit(0);
        }
        close(client_sock);
    }
    close(server_sock);
    return;
}

void start_server(conf * configure)
{
    signal(SIGCHLD, sigchld_handler); // 防止子进程变成僵尸进程

    if ((server_sock = create_server_socket(local_port)) < 0) {
        exit(server_sock);
    }

    server_loop(configure);
    return;
}

void get_pid(char *proces_name)
{
    char bufer[PATH_SIZE];
    char comm[PATH_SIZE];
    char proc_comm_name[PATH_SIZE];
    int num[PATH_SIZE] = { 0 };
    int n = 0;
    FILE *fp;
    DIR *dir;
    struct dirent *ptr;
    dir = opendir("/proc");
    while ((ptr = readdir(dir)) != NULL) {
        if (ptr->d_type == 4 && strcasecmp(ptr->d_name, ".")
            && strcasecmp(ptr->d_name, "..")) {
            bzero(bufer, 0);
            sprintf(comm, "/proc/%s/comm", ptr->d_name);
            if (access(comm, F_OK) == 0) {
                fp = fopen(comm, "r");
                if (fgets(bufer, PATH_SIZE - 1, fp) == NULL) {
                    fclose(fp);
                    continue;
                }
                sscanf(bufer, "%s", proc_comm_name);
                if (!strcmp(proces_name, proc_comm_name)) {
                    num[n] = atoi(ptr->d_name);
                    n += 1;
                }
                fclose(fp);
            }
        }

    }

    n -= 2;                     // 去除最后一个搜索时的本身进程
    for (; n >= 0; n--) {
        printf("\t%d\n", num[n]);
    }

    closedir(dir);
    return;
}

int stop(int signal, char *program_name)
{
    if (signal == 1) {
        struct passwd *pwent = NULL;
        pwent = getpwnam("root");
        return kill_all(15, 1, &program_name, pwent);
    }
    if (signal == 2) {
        get_pid(program_name);
        return 0;
    }

    return 0;
}

int get_executable_path(char *processdir, char *processname, int len)
{
    char *filename;
    if (readlink("/proc/self/exe", processdir, len) <= 0) {
        return -1;
    }
    filename = strrchr(processdir, '/');
    if (filename == NULL)
        return -1;
    ++filename;
    strcpy(processname, filename);
    *filename = '\0';
    return (int)(filename - processdir);
}

int _main(int argc, char *argv[])
{
    // 初始化全局变量
    header_buffer = (char *)malloc(BUF_SIZE);
    len_header_buffer = strlen(header_buffer);

    char *inifile = "conf/cproxy.ini";
    char path[PATH_SIZE] = { 0 };
    char executable_filename[PATH_SIZE] = { 0 };
    (void)get_executable_path(path, executable_filename, sizeof(path));
    inifile = strcat(path, inifile);

    conf *configure = (struct CONF *)malloc(sizeof(struct CONF));
    read_conf(inifile, configure);

    local_port = configure->server_port;

    int opt;
    char optstrs[] = ":l:ds:c:h?";
    while (-1 != (opt = getopt(argc, argv, optstrs))) {
        switch (opt) {
        case 'l':
            local_port = atoi(optarg);
            break;
        case 'd':
            (void)get_executable_path(path, executable_filename, sizeof(path));
            init_daemon(1, 1, configure, path);
            break;
        case 's':
            if (strcasecmp(optarg, "stop") == 0
                || strcasecmp(optarg, "quit") == 0) {
                free_conf(configure);
                free(header_buffer);
                exit(stop(1, executable_filename));
            }
            if (strcasecmp(optarg, "restart") == 0
                || strcasecmp(optarg, "reload") == 0) {
                stop(1, executable_filename);
            }
            if (strcasecmp(optarg, "status") == 0) {
                exit(stop(2, executable_filename));
            }
            break;
        case 'c':
            free_conf(configure); // 如果指定-c参数就释放上次分配的内存
            inifile = optarg;
            read_conf(inifile, configure);
            break;
        case 'h':
        case '?':
            help_information();
            exit(0);
            break;
        default:
            ;
        }
    }

    if (setegid(configure->uid) == -1 || seteuid(configure->uid) == -1) // 设置uid
        exit(1);
    
    start_server(configure);
    free_conf(configure);
    free(header_buffer);
    return 0;
}

int main(int argc, char *argv[])
{
    return _main(argc, argv);
}
