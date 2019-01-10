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
void handle_client(int client_sock, struct sockaddr_in client_addr);
int init_daemon(int nochdir, int noclose);
void sigchld_handler(int signal);
int _main(int argc, char *argv[]);
int replacement_http_head(char *header_buffer, char *remote_host,
                          int *remote_port, int *HTTPS);
