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

#define BUF_SIZE 8192
#define PORT 9606
#define MAX_HEADER_SIZE 8192

char remote_host[128]; 
int remote_port; 
int local_port;
int server_sock; 
int client_sock;
int remote_sock;
char * header_buffer ;

enum 
{
    FLG_NONE = 0,       // 正常数据流不进行编解码
    R_C_DEC = 1,        // 读取客户端数据仅进行解码
    W_S_ENC = 2         // 发送到服务端进行编码
};

static int io_flag;     // 网络io的一些标志位

void handle_client(int client_sock, struct sockaddr_in client_addr);
void forward_header(int destination_sock);
void forward_data(int source_sock, int destination_sock);
void rewrite_header();
int send_data(int socket,char * buffer,int len );
int receive_data(int socket, char * buffer, int len);
void hand_mproxy_info_req(int sock,char * header_buffer) ;
int create_connection() ;
int _main(int argc, char *argv[]) ;


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
                return -1;              // 未知错误

        } else if (numRead == 0) {      // EOF
            if (totRead == 0)           // No bytes read; return 0
                return 0;
            else                        // Some bytes read; add '\0'
                break;

        } else {     
                                      
            if (totRead < n - 1) {      // Discard > (n - 1) bytes
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

int read_header(int fd, void * buffer)
{
    memset(header_buffer,0,MAX_HEADER_SIZE);
    char line_buffer[2048];
    char * base_ptr = header_buffer;

    for(;;)
    {
        memset(line_buffer,0,2048);

        int total_read = readLine(fd,line_buffer,2048);
        if(total_read <= 0)
        {
            return -1;
        }
        // 防止header缓冲区蛮越界
        if(base_ptr + total_read - header_buffer <= MAX_HEADER_SIZE)
        {
           strncpy(base_ptr,line_buffer,total_read); 
           base_ptr += total_read;
        } else 
        {
            return -1;
        }

        // 读到了空行，http头结束
        if(strcmp(line_buffer,"\r\n") == 0 || strcmp(line_buffer,"\n") == 0)
        {
            break;
        }

    }
    return 0;

}

void extract_server_path(const char * header,char * output)
{
    char * p = strstr(header,"GET /");
    if(p) {
        char * p1 = strchr(p+4,' ');
        strncpy(output,p+4,(int)(p1  - p - 4) );
    }
    
}

int extract_host(const char * header)
{
    char * _p = strstr(header,"CONNECT");  // 在 CONNECT 方法中解析 隧道主机名称及端口号
    if(_p)
    {
        char * _p1 = strchr(_p,' ');
        char * _p2 = strchr(_p1 + 1,':');
        char * _p3 = strchr(_p1 + 1,' ');

        if(_p2)
        {
            char s_port[10];
            bzero(s_port,10);
            strncpy(remote_host,_p1+1,(int)(_p2  - _p1) - 1);
            strncpy(s_port,_p2+1,(int) (_p3 - _p2) -1);
            remote_port = atoi(s_port);

        } else 
        {
            strncpy(remote_host,_p1+1,(int)(_p3  - _p1) -1);
            remote_port = 80;
        }
        
        
        return 0;
    }

    char * p = strstr(header,"Host:");
    if(!p) 
    {
        return -1;
    }
    char * p1 = strchr(p,'\n');
    if(!p1) 
    {
        return -1; 
    }

    char * p2 = strchr(p + 5,':'); // 5是指'Host:'的长度
    if(p2 && p2 < p1) 
    {
        int p_len = (int)(p1 - p2 -1);
        char s_port[p_len];
        strncpy(s_port,p2+1,p_len);
        s_port[p_len] = '\0';
        remote_port = atoi(s_port);

        int h_len = (int)(p2 - p -5 -1 );
        strncpy(remote_host,p + 5 + 1  ,h_len); // Host:
        remote_host[h_len] = '\0';
    } else 
    {   
        int h_len = (int)(p1 - p - 5 -1 -1); 
        strncpy(remote_host,p + 5 + 1,h_len);
        remote_host[h_len] = '\0';
        remote_port = 80;
    }

    return 0;
}

// 响应隧道连接请求
int send_tunnel_ok(int client_sock)
{
    char * resp = "HTTP/1.1 200 Connection Established\r\n\r\n";
    int len = strlen(resp);
    char buffer[len+1];
    strcpy(buffer,resp);
    if(send_data(client_sock,buffer,len) < 0)
    {
        return -1;
    }
    return 0;
}

void hand_mproxy_info_req(int sock, char * header) {
    char server_path[255] ;
    char response[8192];
    extract_server_path(header,server_path);

    char info_buf[1024];
    sprintf(response,"HTTP/1.0 200 OK\nServer: CProxy/0.1\n\
                    Content-type: text/html; charset=utf-8\n\n\
                     <html><body>\
                     <pre>%s</pre>\
                     </body></html>\n",info_buf);

    write(sock,response,strlen(response));
}

// 处理客户端的连接
void handle_client(int client_sock, struct sockaddr_in client_addr)
{
    int is_http_tunnel = 0;
    if(strlen(remote_host) == 0) // 未指定远端主机名称从http 请求 HOST 字段中获取
    {  
        if(read_header(client_sock,header_buffer) < 0)
        {
            return;
        } else 
        {
            char * p = strstr(header_buffer,"CONNECT"); // 判断是否是http 隧道请求
            if(p) 
            {
                is_http_tunnel = 1;
            }

            if(strstr(header_buffer,"GET /cproxy") > 0) 
            {
                hand_mproxy_info_req(client_sock,header_buffer);
                return; 
            }

            if(extract_host(header_buffer) < 0) 
            {
                return;
            }
        }
    }

    if ((remote_sock = create_connection()) < 0) {
        return;
    }

    if (fork() == 0) { // 创建子进程用于从客户端转发数据到远端socket接口

        if(strlen(header_buffer) > 0 && !is_http_tunnel) 
        {
            forward_header(remote_sock); //普通的http请求先转发header
        } 
        
        forward_data(client_sock, remote_sock);
        exit(0);
    }

    if (fork() == 0) { // 创建子进程用于转发从远端socket接口过来的数据到客户端

        if(io_flag == W_S_ENC)
        {
            io_flag = R_C_DEC; // 发送请求给服务端进行编码，读取服务端的响应则进行解码
        } else if (io_flag == R_C_DEC)
        {
             io_flag = W_S_ENC; //接收客户端请求进行解码，那么响应客户端请求需要编码
        }

        if(is_http_tunnel)
        {
            send_tunnel_ok(client_sock);
        } 

        forward_data(remote_sock, client_sock);
        exit(0);
    }

    close(remote_sock);
    close(client_sock);
}

void forward_data(int source_sock, int destination_sock) {
    char buffer[BUF_SIZE];
    int n;

    while ((n = receive_data(source_sock, buffer, BUF_SIZE)) > 0)
    {
        send_data(destination_sock, buffer, n);
    }

    shutdown(destination_sock, SHUT_RDWR);
    shutdown(source_sock, SHUT_RDWR);
}

void forward_header(int destination_sock)
{
    rewrite_header();
    int len = strlen(header_buffer);
    send_data(destination_sock,header_buffer,len) ;
}

// 代理中的完整URL转发前需改成 path 的形式
void rewrite_header()
{
    char * p = strstr(header_buffer,"http://");
    char * p0 = strchr(p,'\0');
    char * p5 = strstr(header_buffer,"HTTP/"); // "HTTP/" 是协议标识 如 "HTTP/1.1"
    int len = strlen(header_buffer);
    if(p)
    {
        char * p1 = strchr(p + 7,'/');
        if(p1 && (p5 > p1))
        {
            // 转换url到 path
            memcpy(p,p1,(int)(p0 -p1));
            int l = len - (p1 - p) ;
            header_buffer[l] = '\0';
        } else
        {
            char * p2 = strchr(p,' ');  // GET http://3g.sina.com.cn HTTP/1.1
            memcpy(p + 1,p2,(int)(p0-p2));
            *p = '/';  // url 没有路径使用根
            int l  = len - (p2  - p ) + 1;
            header_buffer[l] = '\0';

        }
    }
}

int receive_data(int socket, char * buffer, int len)
{
    int n = recv(socket, buffer, len, 0);
    if(io_flag == R_C_DEC && n > 0)
    {
        int i; 
        for(i = 0; i< n; i++ )
        {
            buffer[i] ^= 1;
        }
    }

    return n;
}

int send_data(int socket,char * buffer,int len)
{
    if(io_flag == W_S_ENC)
    {
        int i;
        for(i = 0; i < len ; i++)
        {
            buffer[i] ^= 1;
        }
    }
    return send(socket,buffer,len,0);
}

int create_connection() {
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

    if (connect(sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        return -1;
    }

    return sock;
}

int _main(int argc, char *argv[])
{
	header_buffer = (char *) malloc(MAX_HEADER_SIZE);

    int server_sock, optval;
    struct sockaddr_in server_addr;
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        return -1;
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        return -1;
    }
    if (listen(server_sock, 17) < 0) {
        return -1;
    }

	struct sockaddr_in clnt_addr;
	socklen_t clnt_addr_size = sizeof(clnt_addr);
    while (1) {
        int client_socket =
            accept(server_sock, (struct sockaddr *) &clnt_addr,
                &clnt_addr_size);

		if (fork() == 0) { // 创建子进程处理客户端连接请求
            close(server_sock);
            handle_client(client_socket, clnt_addr);
			printf("%s", header_buffer);
            exit(0);
        }
        close(client_socket);
    }
}

int main(int argc, char *argv[])
{
    return _main(argc,argv);
}
