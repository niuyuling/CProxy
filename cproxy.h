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

#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include <errno.h>

#include "iniparser.h"
#define LOG(fmt...)  do { fprintf(stderr,"%s %s ",__DATE__,__TIME__); fprintf(stderr, ##fmt); } while(0)
#define BUF_SIZE 8192
#define MAX_HEADER_SIZE 8192

typedef struct CONF {
    int server_port; // server module
    char *server_pid_file;
	
    int http_port; // http module
    char *http_ip, *http_del, *http_first;

    int https_port; // https module
    char *https_ip, *https_del, *https_first;

    int len_server_pid_file; // length
    int len_http_ip, len_http_del, len_http_first;
    int len_https_ip, len_https_del, len_https_first;
} conf;

void read_conf(char *file, conf * p);
void free_conf(conf * p);

char remote_host[128];
int remote_port;
int local_port;
int server_sock;
int client_sock;
int remote_sock;
char *header_buffer;

ssize_t readLine(int fd, void *buffer, size_t n);
int read_header(int fd, void *buffer);
void extract_server_path(const char *header, char *output);
int extract_host(char *header);
int send_tunnel_ok(int client_sock);
void hand_cproxy_info_req(int sock, char *header_buffer);
void forward_data(int source_sock, int destination_sock);
void forward_header(int destination_sock);
void rewrite_header();
int receive_data(int socket, char *buffer, int len);
int send_data(int socket, char *buffer, int len);
int create_connection();
void handle_client(int client_sock, struct sockaddr_in client_addr, conf * p);
int init_daemon(int nochdir, int noclose, conf *p);
void sigchld_handler(int signal);
int _main(int argc, char *argv[]);
int replacement_http_head(char *header_buffer, char *remote_host,
                          int *remote_port, int *HTTPS, conf * p);

#endif
