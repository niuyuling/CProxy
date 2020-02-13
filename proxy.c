#include "proxy.h"
#include "http.h"
#include "request.h"
#include "timeout.h"
#include "conf.h"
#include "kill.h"
#include "help.h"

#define SERVER_STOP 1
#define SERVER_RELOAD 2
#define SERVER_STATUS 3

struct epoll_event ev, events[MAX_CONNECTION + 1];
int epollfd, server_sock;
conn cts[MAX_CONNECTION];


int create_connection(char *remote_host, int remote_port) {
    struct sockaddr_in server_addr;
    struct hostent *server;
    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }
    
    if ((server = gethostbyname(remote_host)) == NULL) {
        perror("gethostbyname");
        errno = EFAULT;
        return -1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(remote_port);
    if (connect(sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        return -1;
    }
    
    fcntl(sock, F_SETFL, O_NONBLOCK);
    return sock;
}

int create_server_socket(int port) {
    int server_sock;
    int optval = 1;
    struct sockaddr_in server_addr;
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt");
        return -1;
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        perror("bind");
        return -1;
    }
    if (listen(server_sock, 50) < 0) {
        perror("listen");
        return -1;
    }
    return server_sock;
}

void accept_client()
{
    struct epoll_event epollEvent;
    struct sockaddr_in addr;
    conn *client;
    socklen_t addr_len = sizeof(addr);

    // 偶数为客户端,奇数为服务端
    for (client = cts; client - cts < MAX_CONNECTION; client += 2)
        if (client->fd < 0)
            break;
    if (client - cts >= MAX_CONNECTION)
        return;
    client->timer = (client+1)->timer = 0;
    client->fd = accept(server_sock, (struct sockaddr *)&addr, &addr_len);
    if (client->fd < 0)
        return;
    fcntl(client->fd, F_SETFL, O_NONBLOCK);
    epollEvent.events = EPOLLIN|EPOLLET;
    epollEvent.data.ptr = client;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, client->fd, &epollEvent);
}

void start_server(conf *configure)
{
    int n;
    pthread_t thId;
    if (timeout_minute)
        pthread_create(&thId, NULL, &close_timeout_connectionLoop, NULL);

    while (1) {
        n = epoll_wait(epollfd, events, MAX_CONNECTION, -1);
        while (n-- > 0) {
            if (events[n].data.fd == server_sock) {
                accept_client();
            } else {
                if(events[n].events & EPOLLIN) {
                    tcp_in((conn *) events[n].data.ptr, configure);
                }
                if (events[n].events & EPOLLOUT) {
                    tcp_out((conn *) events[n].data.ptr);
                }
            }
        }
    }
    close(epollfd);
}

int
process_signal(int signal, char *process_name)
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
        if (ptr->d_type == DT_DIR && strcasecmp(ptr->d_name, ".")
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
                if (!strcmp(process_name, proc_comm_name)) {
                    num[n] = atoi(ptr->d_name);
                    n += 1;
                }
                fclose(fp);
            }
        }

    }
    n -= 2;                     // 去除最后一个搜索时的本身进程
    for (; n >= 0; n--) {
        if (signal == SERVER_STATUS)
            printf("\t%d\n", num[n]);
    }
    if (signal == SERVER_STOP || signal == SERVER_RELOAD) {
        //kill(num[n], SIGTERM);
        struct passwd *pwent = NULL;
        pwent = getpwnam("root");
        return kill_all(15, 1, &process_name, pwent);
    }
    closedir(dir);
    return 0;
}

int get_executable_path(char *processdir, char *processname, int len)
{
    char *filename;
    if (readlink("/proc/self/exe", processdir, len) <= 0)
        return -1;
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
    int opt, i, process;
    char path[PATH_SIZE] = { 0 };
    char executable_filename[PATH_SIZE] = { 0 };
    (void)get_executable_path(path, executable_filename, sizeof(path));
    char *inifile = "/CProxy.ini";
    inifile = strcat(path, inifile);
    conf *configure = (struct CONF *)malloc(sizeof(struct CONF));
    read_conf(inifile, configure);

    timeout_minute = 0;
    if (configure->timer > 0)
        timeout_minute = configure->timer;
    process = 2;
    if (configure->process > 0)
        process = configure->process;

    char optstrs[] = ":l:f:t:p:c:s:h?";
    char *p = NULL;
    while (-1 != (opt = getopt(argc, argv, optstrs))) {
        switch (opt) {
        case 'l':
            p = strchr(optarg, ':');
            if (p) {
                strncpy(local_host, optarg, p - optarg);
                local_port = atoi(p + 1);
            } else {
                strncpy(local_host, optarg, strlen(local_host));
            }
            break;
        case 'f':
            p = strchr(optarg, ':');
            if (p) {
                strncpy(remote_host, optarg, p - optarg);
                remote_port = atoi(p + 1);
            } else {
                strncpy(remote_host, optarg, strlen(remote_host));
            }
            break;
        case 't':
            timeout_minute = (time_t)atoi(optarg);
            break;
        case 'p':
            process = atoi(optarg);
            break;
        case 'c':
            free_conf(configure);
            inifile = optarg;
            read_conf(inifile, configure);
            break;
        case 's':
            if (strcasecmp(optarg, "stop") == 0 || strcasecmp(optarg, "quit") == 0) {
                free_conf(configure);
                exit(process_signal(SERVER_STOP, executable_filename));
            }
            if (strcasecmp(optarg, "restart") == 0 || strcasecmp(optarg, "reload") == 0)
                process_signal(SERVER_RELOAD, executable_filename);
            if (strcasecmp(optarg, "status") == 0)
                exit(process_signal(SERVER_STATUS, executable_filename));
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
    
    server_sock = create_server_socket(configure->server_port);
    signal(SIGPIPE, SIG_IGN);  //忽略PIPE信号
    
    memset(cts, 0, sizeof(cts));
    for (i = MAX_CONNECTION; i--; )
        cts[i].fd = -1;
    //为服务端的结构体分配内存
    for (i = 1; i < MAX_CONNECTION; i += 2)
    {
        cts[i].header_buffer = (char *)malloc(BUFFER_SIZE);
        if (cts[i].header_buffer == NULL)
        {
            fputs("out of memory.", stderr);
            exit(1);
        }
    }

    if (daemon(1, 1)) {
        perror("daemon");
        return 1;
    }

    while (process-- > 0 && fork() == 0)

    epollfd = epoll_create(MAX_CONNECTION);
    if (epollfd == -1) {
        perror("epoll_create");
        exit(1);
    }
    static struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = server_sock;
    if (-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, server_sock, &event)) {
        exit(1);
    }

    if (setegid(configure->uid) == -1 || seteuid(configure->uid) == -1) // 设置uid
        exit(1);
    
    start_server(configure);
    return 0;
}

int main(int argc, char *argv[])
{
    return _main(argc, argv);
}

