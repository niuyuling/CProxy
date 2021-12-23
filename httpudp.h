#ifndef HTTPUDP_H
#define HTTPUDP_H

#include "main.h"

/* 定义TPROXY模块需要的选项，有些编译器不带这些定义 */
#ifndef IP_TRANSPARENT
#define IP_TRANSPARENT 19
#endif
#ifndef IP_RECVORIGDSTADDR
#define IP_RECVORIGDSTADDR 20
#endif
#ifndef IP_ORIGDSTADDR
#define IP_ORIGDSTADDR 20
#endif
//默认使用HTTPS模块
//#define HTTPUDP_REQUEST "GET / HTTP/1.1\r\nHost: [H]\r\nConnection: Upgrade\r\nSec-WebSocket-Key: ChameleonProxy httpUDP Client\r\nSec-WebSocket-Version: "VERSION"\r\nUpgrade: websocket\r\nProxy-Connection: Keep-Alive\r\n\r\n"

struct httpudp {
    struct sockaddr_in dst;
    char *http_request, *original_http_request; //original_http_request为初始化生成的请求头，用来配合use_hdr语法
    int http_request_len, original_http_request_len;
    unsigned encodeCode,        //数据编码传输
     httpsProxy_encodeCode;     //CONNECT代理编码
};

extern void udp_timeout_check();
extern void *udp_loop(void *nullPtr);
extern void udp_init();

extern struct httpudp udp;

#endif
