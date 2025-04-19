#ifndef WS_PARSER_H
#define WS_PARSER_H

#include "http_parser.h"
#include <stdint.h>
#include <netinet/in.h>
#include <sys/types.h>
#define WS_KEY_LENGTH 24 //经过Base64编码后，16字节的数据会变成24个字符

/* WebSocket帧头结构体*/
typedef struct {
    unsigned char fin;
    unsigned char opcode;
    unsigned char mask;
    uint64_t payload_len;
    unsigned char masking_key[4];
} WebSocketFrameHeader;

//解析websocket请求
int ws_handler(int client_sock, struct sockaddr_in client_addr, RequestPDU *request);

/*解析http请求头并判断是否合法*/
int parse_header(int client_sock,RequestPDU* request);

/* WebSocket帧解析 调用时需注意释放payload的内存*/
int parse_ws_frame(unsigned char* buffer, ssize_t len, WebSocketFrameHeader* header, unsigned char** payload) ;

/* 构造WebSocket帧 调用时需注意释放frame的内存*/
unsigned char* build_ws_frame(const char* message, size_t* frame_len);

/*与后端服务器进行连接*/
int connect_to_backend();

/*安全关闭 WebSocket 连接（双向握手）*/
void safe_ws_close(int client_sock, int backend_sock);

// 构造 Close 帧（含状态码）
void send_close_frame(int sock, uint16_t code );




#endif // WS_PARSER_H