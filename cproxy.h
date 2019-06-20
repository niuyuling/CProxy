#ifndef CPROXY_H
#define CPROXY_H

#include <arpa/inet.h>
#include <errno.h>
#include <libgen.h>
#include <netdb.h>
#include <resolv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <dirent.h>

#define PATH_SIZE 270
#define BUF_SIZE 8192
#define BUF_SIZES 1024

#define SERVER_SOCKET_ERROR -1
#define SERVER_SETSOCKOPT_ERROR -2
#define SERVER_BIND_ERROR -3
#define SERVER_LISTEN_ERROR -4
#define CLIENT_SOCKET_ERROR -5
#define CLIENT_RESOLVE_ERROR -6
#define CLIENT_CONNECT_ERROR -7

char remote_host[128];
int remote_port;
int local_port;

int server_sock;
int client_sock;
int remote_sock;

char *header_buffer;
int len_header_buffer;
char *complete_data;
int len_complete_data;

int SIGN;

// 配置文件结构
typedef struct CONF {
    int server_port;            // server module
    char *server_pid_file;

    int http_port;              // http module
    char *http_ip, *http_del, *http_first;

    int https_port;             // https module
    char *https_ip, *https_del, *https_first;

    int len_server_pid_file;    // length
    int len_http_ip, len_http_del, len_http_first;
    int len_https_ip, len_https_del, len_https_first;
    
    char *http_strrep, *http_regrep, *https_strrep, *https_regrep;
    char *http_strrep_aim, *http_strrep_obj;
    char *https_strrep_aim, *https_strrep_obj;
    
    char *http_regrep_aim, *http_regrep_obj;
    char *https_regrep_aim, *https_regrep_obj;
    
    int len_http_strrep, len_http_regrep;
    int len_https_strrep, len_https_regrep;
} conf;

// 请求类型
#define OTHER 1
#define HTTP 2
#define HTTP_OTHERS 3
#define HTTP_CONNECT 4

char *read_data(int client_sock, char *data, int *data_len);
void servertoclient(int remote_sock, int client_sock, char *complete_data, int *len_complete_data);
void clienttoserver(int remote_sock, char *complete_data, int *len_complete_data);
void handle_client(int client_sock, struct sockaddr_in client_addr, conf *configure);
int send_data(int socket, char *buffer, int len);
int receive_data(int socket, char *buffer, int len);
void forward_data(int source_sock, int destination_sock);
int create_connection(conf *configure, int SIGN);
int create_server_socket(int port);
int init_daemon(int nochdir, int noclose, conf *configure, char *path);
void sigchld_handler(int signal);
void server_loop(conf *configure);
void start_server(conf *configure);
int _main(int argc, char *argv[]);
void read_conf(char *file, conf *p);
void free_conf(conf *p);
int extract_host(char *header);
int replacement_http_head(char *header_buffer, char *remote_host, int *remote_port, int *SIGN, conf *p);
uint8_t request_type(char *req);
void forward_header(int destination_sock);
void rewrite_header();
char help_information(void);

#endif

