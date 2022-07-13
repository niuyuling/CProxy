#include "main.h"
#include "http_proxy.h"
#include "http_request.h"
#include "httpdns.h"
#include "httpudp.h"
#include "conf.h"

#define SERVER_TYPE_STOP 1
#define SERVER_TYPE_RELOAD 2
#define SERVER_TYPE_STATUS 3
#define SERVICE_TYPE_STATUS_NOT_PRINT 4

struct global global;
struct tcp_mode http, https;
struct save_header *saveHdrs;
char *default_ssl_request;
int default_ssl_request_len;
uint16_t tcp_listen_port;

tcp *http_head_strrep;
tcp *http_head_regrep;

tcp *https_head_strrep;
tcp *https_head_regrep;

struct epoll_event ev, events[MAX_CONNECTION + 1];
int epollfd, server_sock, server_sock6, local_port, process;
conn_t cts[MAX_CONNECTION];
char local_host[CACHE_SIZE];

static char help_information(void)
{
    static const char name[] = "C";
    static const char subject[] = "Proxy Server";
    static const struct {
        const char *email;
    } author = {
        "AIXIAO@AIXIAO.ME",
    };

    static const char usage[] = "Usage: [-?h] [-s signal] [-c filename]";
    static const char *s_help[] = {
        "",
        "Options:",
        "    -s --signal            : send signal to a master process: stop, quit, restart, reload, status",
        "    -c --config            : set configuration file, default: CProxy.conf",
        "    -? -h --? --help       : help information",
        "",
        0
    };

    fprintf(stderr, "%s %s\n", name, subject);
    fprintf(stderr, "Author: %s\n", author.email);
    fprintf(stderr, "%s\n", usage);

    int l;
    for (l = 0; s_help[l]; l++) {
        fprintf(stderr, "%s\n", s_help[l]);
    }

    BUILD("Compile、link.\n");

    return 0;
}

int create_server_socket(int port)
{
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
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        perror("bind");
        return -1;
    }
    if (listen(server_sock, 500) < 0) {
        perror("listen");
        return -1;
    }
    return server_sock;
}

int create_server_socket6(int port)
{
    int server_sock;
    int optval = SO_REUSEADDR;
    struct sockaddr_in6 server_addr;
    if ((server_sock = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt");
        return -1;
    }

    if (setsockopt(server_sock, IPPROTO_IPV6, IPV6_V6ONLY, &optval, sizeof(optval)) < 0) {
        perror("setsockopt");
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_port = htons(port);
    server_addr.sin6_addr = in6addr_any;

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in6)) != 0) {
        perror("bind");
        return -1;
    }

    if (listen(server_sock, 500) < 0) {
        perror("listen");
        return -1;
    }

    return server_sock;
}

void accept_client()
{
    struct epoll_event epollEvent;
    struct sockaddr_in addr;
    conn_t *client;
    socklen_t addr_len = sizeof(addr);

    // 偶数为客户端,奇数为服务端
    for (client = cts; client - cts < MAX_CONNECTION; client += 2)
        if (client->fd < 0)
            break;
    if (client - cts >= MAX_CONNECTION)
        return;
    client->timer = (client + 1)->timer = 0;
    client->fd = accept(server_sock, (struct sockaddr *)&addr, &addr_len);
    if (client->fd < 0)
        return;
    fcntl(client->fd, F_SETFL, O_NONBLOCK);
    epollEvent.events = EPOLLIN | EPOLLET;
    epollEvent.data.ptr = client;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, client->fd, &epollEvent);
}

void accept_client6()
{
    struct epoll_event epollEvent;
    struct sockaddr_in6 addr;
    conn_t *client;
    socklen_t addr_len = sizeof(addr);

    // 偶数为客户端,奇数为服务端
    for (client = cts; client - cts < MAX_CONNECTION; client += 2)
        if (client->fd < 0)
            break;
    if (client - cts >= MAX_CONNECTION)
        return;
    client->timer = (client + 1)->timer = 0;
    client->fd = accept(server_sock6, (struct sockaddr *)&addr, &addr_len);
    if (client->fd < 0)
        return;
    fcntl(client->fd, F_SETFL, O_NONBLOCK);
    epollEvent.events = EPOLLIN | EPOLLET;
    epollEvent.data.ptr = client;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, client->fd, &epollEvent);
}

void *tcp_loop(void *p)
{
    conf *configure = (conf *) p;
    int n;

    ev.events = EPOLLIN;
    ev.data.fd = server_sock;
    if (-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, server_sock, &ev)) {
        exit(1);
    }

    ev.events = EPOLLIN;
    ev.data.fd = server_sock6;
    if (-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, server_sock6, &ev)) {
        exit(1);
    }

    while (1) {
        n = epoll_wait(epollfd, events, MAX_CONNECTION, -1);
        while (n-- > 0) {
            if (events[n].data.fd == server_sock6) {
                accept_client6();
            } else if (events[n].data.fd == server_sock) {
                accept_client();
            } else {
                if (events[n].events & EPOLLIN) {
                    tcp_in((conn_t *) events[n].data.ptr, configure);
                }
                if (events[n].events & EPOLLOUT) {
                    tcp_out((conn_t *) events[n].data.ptr);
                }
            }
        }
    }

    return NULL;
}

int process_signal(int signal, char *process_name)
{
    char bufer[CACHE_SIZE];
    char comm[CACHE_SIZE];
    char proc_comm_name[CACHE_SIZE];
    FILE *fp;
    DIR *dir;
    struct dirent *ptr;
    pid_t self_pid;

    self_pid = getpid();
    dir = opendir("/proc");
    bzero(bufer, 0);
    bzero(comm, 0);
    bzero(proc_comm_name, 0);
    while ((ptr = readdir(dir)) != NULL) {
        if (ptr->d_type != DT_DIR)
            continue;
        if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0 || atoi(ptr->d_name) == self_pid)
            continue;

        sprintf(comm, "/proc/%s/comm", ptr->d_name);
        if (access(comm, F_OK) == 0) {
            fp = fopen(comm, "r");
            if (fgets(bufer, CACHE_SIZE - 1, fp) == NULL) {
                fclose(fp);
                continue;
            }
            sscanf(bufer, "%s", proc_comm_name);
            if (!strcmp(process_name, proc_comm_name)) {
                if (signal == SERVER_TYPE_STOP)
                    kill(atoi(ptr->d_name), SIGTERM);
                else {
                    closedir(dir);
                    if (signal != SERVICE_TYPE_STATUS_NOT_PRINT)
                        printf("\t%d\n", atoi(ptr->d_name));;
                    return 0;
                }
            }
            fclose(fp);
        }
    }
    closedir(dir);

    if (signal == SERVER_TYPE_STATUS) ;
    else if (signal == SERVICE_TYPE_STATUS_NOT_PRINT)
        return 1;

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

void server_ini()
{
    signal(SIGPIPE, SIG_IGN);   // 忽略PIPE信号

    if (daemon(1, 1)) {
        perror("daemon");
        return;
    }

    while (process-- > 1 && fork() == 0);
}

// 监听一个UDP接口
int udp_listen(char *ip, int port)
{
    struct sockaddr_in addr;
    int fd, opt = 1;

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("udp socket");
        exit(1);
    }
    setsockopt(fd, SOL_IP, IP_TRANSPARENT, &opt, sizeof(opt));
    setsockopt(fd, SOL_IP, IP_RECVORIGDSTADDR, &opt, sizeof(opt));
    memset(&addr, 0, sizeof(addr));
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("udp bind");
        exit(1);
    }

    return fd;
}

void *timeout_check(void *nullPtr)
{
    while (1) {
        sleep(60);
        if (server_sock >= 0 || server_sock6 >= 0)
            tcp_timeout_check(NULL);
        if (global.dns_listen_fd >= 0)
            dns_timeout_check();
        if (global.udp_listen_fd >= 0)
            udp_timeout_check();
    }

    return NULL;
}

void initialize(conf * configure)
{
    int i;
    char *p;

    memset(cts, 0, sizeof(cts));
    for (i = MAX_CONNECTION; i--;)
        cts[i].fd = -1;
    // 为服务端的结构体分配内存
    for (i = 1; i < MAX_CONNECTION; i += 2) {
        cts[i].ready_data = (char *)malloc(BUFFER_SIZE);
        if (cts[i].ready_data == NULL) {
            fputs("out of memory.", stderr);
            exit(1);
        }
    }

    memset(&global, 0, sizeof(global));
    memset(&httpdns, 0, sizeof(httpdns));
    memset(&https, 0, sizeof(https));
    memset(&udp, 0, sizeof(udp));
    saveHdrs = NULL;
    httpdns.dst.sin_family = AF_INET;
    udp.dst.sin_family = AF_INET;
    global.tcp_listen_fd = global.dns_listen_fd = global.udp_listen_fd = global.uid = -1;

    // httpdns module
    global.dns_listen_fd = udp_listen((char *)"127.0.0.1", configure->dns_listen);
    if ((p = strchr(configure->httpdns_addr, ':')) != NULL) {
        *p = '\0';
        httpdns.dst.sin_port = htons(atoi(p + 1));
    } else {
        httpdns.dst.sin_port = htons(80);
    }
    httpdns.dst.sin_addr.s_addr = inet_addr(configure->httpdns_addr);
    httpdns.http_req_len = configure->httpdns_http_req_len;
    copy_new_mem(configure->httpdns_http_req, httpdns.http_req_len, &httpdns.http_req);

    // httpudp module
    global.udp_listen_fd = udp_listen((char *)"0.0.0.0", configure->udp_listen);
    if ((p = strchr(configure->httpudp_addr, ':')) != NULL) {
        *p = '\0';
        udp.dst.sin_port = htons(atoi(p + 1));
    } else {
        udp.dst.sin_port = htons(80);
    }
    udp.dst.sin_addr.s_addr = inet_addr(configure->httpudp_addr);
    udp.http_request_len = configure->httpudp_http_req_len;
    copy_new_mem(configure->httpudp_http_req, udp.http_request_len, &udp.http_request);

    // global module
    server_sock = create_server_socket(configure->tcp_listen); // IPV4
    server_sock6 = create_server_socket6(configure->tcp_listen); // IPV6

}

void thread_loop(conf * configure)
{
    pthread_t thread_id = 0;
    /*
    sigset_t signal_mask;
    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGPIPE); // 忽略PIPE信号
    if (pthread_sigmask(SIG_BLOCK, &signal_mask, NULL) != 0) {
        printf("block sigpipe error\n");
    }
    */
    
    if (global.timeout_m)
        pthread_create(&thread_id, NULL, &timeout_check, NULL);
    
    if (server_sock >= 0)
    {
        if (global.dns_listen_fd >= 0) {
            dns_init();
            pthread_create(&thread_id, NULL, &dns_loop, NULL);
        }
        if (global.udp_listen_fd >= 0) {
            udp_init();
            //udp_loop(NULL);
            pthread_create(&thread_id, NULL, &udp_loop, NULL);
        }
        
        if (pthread_create(&thread_id, NULL, &tcp_loop, (void *)configure) != 0)
            perror("pthread_create");
    }
    
    if (global.dns_listen_fd >= 0)
    {
        dns_init();
        if (global.udp_listen_fd >= 0)
        {
            udp_init();
            pthread_create(&thread_id, NULL, &udp_loop, NULL);
        }
        dns_loop(NULL);
    }

    udp_init();
    udp_loop(NULL);

    //pthread_join(thread_id, NULL);
    //pthread_exit(NULL);

    /*  线程分离
       pthread_attr_t attr;
       pthread_attr_init(&attr);
       pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
       pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
       pthread_create(&thread_id, &attr, &tcp_timeout_check, NULL);
       pthread_create(&thread_id, &attr, &tcp_loop, (void *)configure);
       pthread_exit(NULL);
     */
}

void _main(int argc, char *argv[])
{
    char *p = NULL;
    int longindex = 0;
    int opt;
    char path[CACHE_SIZE] = { 0 };
    char executable_filename[CACHE_SIZE] = { 0 };
    (void)get_executable_path(path, executable_filename, sizeof(path));
    char *inifile = "/CProxy.conf";
    struct rlimit rt;
    inifile = strcat(path, inifile);
    conf *configure = (struct CONF *)malloc(sizeof(struct CONF));
    memset(configure, 0, sizeof(struct CONF));
    read_conf(inifile, configure);

    sslEncodeCode = 0;          // 默认SSL不转码

    char optstring[] = ":l:f:t:p:c:e:s:h?";
    static struct option longopts[] = {
        { "local_address", required_argument, 0, 'l' },
        { "remote_address", required_argument, 0, 'f' },
        { "timeout", required_argument, 0, 't' },
        { "process", required_argument, 0, 'p' },
        { "config", required_argument, 0, 'c' },
        { "coding", required_argument, 0, 'e' },
        { "signal", required_argument, 0, 's' },
        { "help", no_argument, 0, 'h' },
        { "?", no_argument, 0, '?' },
        { 0, 0, 0, 0 }
    };
    while (-1 != (opt = getopt_long(argc, argv, optstring, longopts, &longindex))) {
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
            timeout_minute = (time_t) atoi(optarg); // 如果指定-t,优先使用参数提供的值(输入值 > 配置文件读取的值)
            break;
        case 'p':
            process = atoi(optarg);
            break;
        case 'c':
            free_tcp(&http_head_strrep);
            free_tcp(&http_head_regrep);
            free_tcp(&https_head_strrep);
            free_tcp(&https_head_regrep);
            free_conf(configure);
            memset(configure, 0, sizeof(struct CONF));
            read_conf(optarg, configure);
            break;
        case 'e':
            sslEncodeCode = atoi(optarg);
            break;
        case 's':
            if (strcasecmp(optarg, "stop") == 0 || strcasecmp(optarg, "quit") == 0) {
                free_conf(configure);
                exit(process_signal(SERVER_TYPE_STOP, executable_filename));
            }
            if (strcasecmp(optarg, "restart") == 0 || strcasecmp(optarg, "reload") == 0) {
                process_signal(SERVER_TYPE_STOP, executable_filename);
                while (process_signal(SERVICE_TYPE_STATUS_NOT_PRINT, executable_filename) == 0) ;
            }
            if (strcasecmp(optarg, "status") == 0)
                exit(process_signal(SERVER_TYPE_STATUS, executable_filename));
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
    
    
    signal(SIGPIPE, SIG_IGN);
#if DAEMON
    // 守护进程
    int nochdir = 1;
    int noclose = 1;
    int pid;
    if ((pid = fork()) < 0) {
        return;
    } else if (0 != pid) {
        free_tcp(&http_head_strrep);
        free_tcp(&http_head_regrep);
        free_tcp(&https_head_strrep);
        free_tcp(&https_head_regrep);
        free_conf(configure);
        free(configure);
        exit(0);
    }

    if (setsid() < 0) {
        return;
    }

    signal(SIGHUP, SIG_IGN);
    if ((pid = fork()) < 0) {
        return;
    } else if (0 != pid) {
        free_tcp(&http_head_strrep);
        free_tcp(&http_head_regrep);
        free_tcp(&https_head_strrep);
        free_tcp(&https_head_regrep);
        free_conf(configure);
        free(configure);
        exit(0);
    }

    if (0 == nochdir) {
        chdir("/");
    }

    if (0 == noclose) {
        freopen("/dev/null", "r", stdin);
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
    }
#endif



    // 反转链表，使读取的配置正序
    http_head_strrep = local_reverse(http_head_strrep);
    http_head_regrep = local_reverse(http_head_regrep);

    https_head_strrep = local_reverse(https_head_strrep);
    https_head_regrep = local_reverse(https_head_regrep);

/*
    print_tcp(https_head_strrep);
    print_tcp(https_head_regrep);
    free_tcp(&https_head_strrep);
    free_tcp(&https_head_regrep);
    
    print_tcp(http_head_strrep);
    print_tcp(http_head_regrep);
    free_tcp(&http_head_strrep);
    free_tcp(&http_head_regrep);
    
    free_conf(configure);
    free(configure);
    exit(0);
*/

    
    rt.rlim_max = rt.rlim_cur = MAX_CONNECTION * 2; // 设置每个进程允许打开的最大文件数
    if (setrlimit(RLIMIT_NOFILE, &rt) == -1)
        perror("setrlimit");
    initialize(configure);
    
    
    if (setegid(configure->uid) == -1 || seteuid(configure->uid) == -1) // 设置uid
        exit(1);
        
    while (configure->process-- > 1 && (child_pid = fork()) == 0);
    
    epollfd = epoll_create(MAX_CONNECTION);
    if (epollfd == -1) {
        perror("epoll_create");
        exit(1);
    }

    thread_loop(configure);

    return;
}

int main(int argc, char *argv[], char **envp)
{
    _main(argc, argv);
    return 0;
}
