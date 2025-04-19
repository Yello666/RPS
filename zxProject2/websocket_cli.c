#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <time.h>

#define BUFFER_SIZE 4096
#define WS_KEY_LENGTH 24
char handshake[BUFFER_SIZE];//存放websocket握手帧
char message[BUFFER_SIZE];//存放回声信息
char buffer[BUFFER_SIZE];//接收握手响应
/* WebSocket帧头结构体 */
typedef struct {
    unsigned char fin;
    unsigned char opcode;
    unsigned char mask;
    uint64_t payload_len;
    unsigned char masking_key[4];
} WebSocketFrameHeader;

/* 生成随机掩码密钥 */
void generate_mask_key(unsigned char mask[4]) {
    srand(time(NULL));
    for(int i=0; i<4; i++) {
        mask[i] = rand() % 256;
    }
}

/* Base64编码 */
char* base64_encode(const unsigned char* input, int length) {
    BIO *bmem, *b64;
    BUF_MEM *bptr;

    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_write(b64, input, length);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    char* buff = (char*)malloc(bptr->length + 1);
    memcpy(buff, bptr->data, bptr->length);
    buff[bptr->length-1] = '\0'; // 去除换行符
    
    BIO_free_all(b64);
    return buff;
}

/* 构造WebSocket帧 */
unsigned char* build_ws_frame(const char* message, size_t* frame_len) {
    size_t msg_len = strlen(message);
    unsigned char* frame = NULL;
    size_t header_len;
    unsigned char mask_key[4];
    
    generate_mask_key(mask_key);
    
    // 基本头部（客户端必须设置掩码）
    if(msg_len <= 125) {
        header_len = 6; // 2字节头 + 4字节掩码
        frame = malloc(header_len + msg_len);
        frame[0] = 0x81; // FIN=1, Opcode=1
        frame[1] = 0x80 | msg_len; // Mask=1
    } 
    else if(msg_len <= 65535) {
        header_len = 8; // 4字节头 + 4字节掩码
        frame = malloc(header_len + msg_len);
        frame[0] = 0x81;
        frame[1] = 0x80 | 126; // Mask=1
        frame[2] = (msg_len >> 8) & 0xFF;
        frame[3] = msg_len & 0xFF;
    } 
    else {
        header_len = 14; // 10字节头 + 4字节掩码
        frame = malloc(header_len + msg_len);
        frame[0] = 0x81;
        frame[1] = 0x80 | 127; // Mask=1
        for(int i=0; i<8; i++) {
            frame[9 - i] = (msg_len >> (8 * i)) & 0xFF;
        }
    }
    
    // 添加掩码密钥
    memcpy(frame + header_len - 4, mask_key, 4);
    
    // 应用掩码到数据
    unsigned char* payload_ptr = frame + header_len;
    for(size_t i=0; i<msg_len; i++) {
        payload_ptr[i] = message[i] ^ mask_key[i % 4];
    }
    
    *frame_len = header_len + msg_len;
    return frame;
}

/* 解析WebSocket帧 */
int parse_ws_frame(unsigned char* buffer, ssize_t len, WebSocketFrameHeader* header, unsigned char** payload) {
    if(len < 2) return -1;

    // 解析第一个字节
    header->fin = (buffer[0] & 0x80) >> 7;
    header->opcode = buffer[0] & 0x0F;

    // 解析第二个字节
    header->mask = (buffer[1] & 0x80) >> 7;
    uint64_t payload_len = buffer[1] & 0x7F;

    int offset = 2;
    
    // 处理扩展长度
    if(payload_len == 126) {
        if(len < 4) return -1;
        payload_len = (buffer[2] << 8) | buffer[3];
        offset += 2;
    } else if(payload_len == 127) {
        if(len < 10) return -1;
        payload_len = (uint64_t)buffer[2] << 56 |
                     (uint64_t)buffer[3] << 48 |
                     (uint64_t)buffer[4] << 40 |
                     (uint64_t)buffer[5] << 32 |
                     (uint64_t)buffer[6] << 24 |
                     (uint64_t)buffer[7] << 16 |
                     (uint64_t)buffer[8] << 8  |
                     (uint64_t)buffer[9];
        offset += 8;
    }

    // 服务端帧不应该有掩码
    if(header->mask) {
        fprintf(stderr, "Server frame should not be masked\n");
        return -1;
    }

    // 验证负载长度
    if(len < offset + payload_len) return -1;

    // 提取负载数据
    *payload = malloc(payload_len + 1);
    memcpy(*payload, buffer + offset, payload_len);
    (*payload)[payload_len] = '\0';
    
    return 0;
}

/*（业务函数）读取命令行输入并发送到服务器*/
int client_biz(int sockfd){
    printf("Enter message (q to quit): ");
    // char message[BUFFER_SIZE];
    fgets(message, BUFFER_SIZE, stdin);
    message[strcspn(message, "\n")] = '\0'; // 去除换行符
    
    if(strncmp(message, "q",1) == 0){
        printf("[cli] exit\n");
        return -1;
    }

    // 构造并发送WebSocket帧
    size_t frame_len;
    unsigned char* frame = build_ws_frame(message, &frame_len);
    if(!frame){
        perror("build ws frame failed");
        return -1;
    }
    if(send(sockfd, frame, frame_len, 0)<0){
        perror("send ws frame failed");
        free(frame);
        return -1;
    }
    free(frame);

    // 接收响应
    size_t len = recv(sockfd, buffer, BUFFER_SIZE, 0);
    if(len <= 0){
        perror("rcv ws frame failed");
        return -1;
    }

    WebSocketFrameHeader header;
    unsigned char* payload;
    if(parse_ws_frame((unsigned char*)buffer, len, &header, &payload) == 0) {
        printf("Received echo: %s\n", payload);
        free(payload);
        return 1;
    } else{
        printf("parse ws farme failed");
        free(payload);
        return -1;
    }
    return 1;
}
int main(int argc, char* argv[]) {
    //参数为serverip，server端口
    if(argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        exit(1);
    }
    //接收命令行参数
    char server_ip[20]="";
    char server_port[20]="";
    strcpy(server_ip, argv[1]);
    strcpy(server_port, argv[2]);
       
    // 创建socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd<0){
        perror("socket failed\n");
        return EXIT_FAILURE;
    }
    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(atoi(server_port))
    };
    inet_pton(AF_INET, server_ip, &serv_addr.sin_addr);

    // 连接服务器
    if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connect failed");
        close(sockfd);
        return EXIT_FAILURE;
    }
    printf("[cli] connected to server(%s:%s)!\n",server_ip,server_port);
    printf("ws handshaking...\n");
    // 生成握手请求
    char client_key[WS_KEY_LENGTH];
    for(int i=0; i<WS_KEY_LENGTH-1; i++) {
        client_key[i] = 'A' + (rand() % 26);
    }
    client_key[WS_KEY_LENGTH-1] = '\0';
    
    //char handshake[BUFFER_SIZE];
    snprintf(handshake, BUFFER_SIZE,
        "GET / HTTP/1.1\r\n"
        "Host: %s:%s\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Key: %s\r\n\r\n",
        server_ip, server_port, client_key);

    // 发送握手请求
    printf("sending handshake request\n");
    printf("Handshake request:\n%s", handshake);
    if(send(sockfd, handshake, strlen(handshake), 0)<0){
        perror("ws handshake send failed\n");
        close(sockfd);
        return EXIT_FAILURE;
    }

    // 接收握手响应
    printf("receive response!\n");
    ssize_t len = recv(sockfd, buffer, BUFFER_SIZE, 0);
    if(len<0){
        perror("ws handshake recv failed\n");
        close(sockfd);
        return EXIT_FAILURE;
    }
    buffer[len] = '\0';

    // 验证握手响应
    if(!strstr(buffer, "101 Switching Protocols")) {
        fprintf(stderr, "Handshake failed\n");
        close(sockfd);
        exit(1);
    }

    printf("Connected to WebSocket server\n");

    // 消息循环
    while(1) {
        if(client_biz(sockfd)<0){
            break;
        }
    }
    close(sockfd);
    return 0;
}