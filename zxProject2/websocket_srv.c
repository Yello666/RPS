#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include<arpa/inet.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include<errno.h>
// #include "include/ws_parser.h"

#define BUFFER_SIZE 4096
#define WS_KEY_LENGTH 24 //经过Base64编码后，16字节的数据会变成24个字符
char buffer[BUFFER_SIZE];
char client_key[WS_KEY_LENGTH + 1];
char response[BUFFER_SIZE];

/* WebSocket帧头结构体*/
typedef struct {
    unsigned char fin;
    unsigned char opcode;
    unsigned char mask;
    uint64_t payload_len;
    unsigned char masking_key[4];
} WebSocketFrameHeader;
/* Base64编码函数 */
char* base64_encode(const unsigned char* input, int length) {
    BIO* bmem = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    b64 = BIO_push(b64, bmem);
    BIO_write(b64, input, length);
    BIO_flush(b64);
    
    char* output = malloc(BUFFER_SIZE);
    int len = BIO_read(bmem, output, BUFFER_SIZE);
    output[len-1] = '\0'; // 去除末尾换行符
    BIO_free_all(b64);
    return output;
}

/* WebSocket握手响应生成 */
int ws_handshake(int client_fd, const char* buffer) {
    printf("ws handshaking..\n");
    int has_upgrade = 0;
    int has_connection_upgrade = 0;
    // char client_key[WS_KEY_LENGTH + 1] = {0};
    memset(client_key,0,sizeof(client_key));
    char *request = strdup(buffer);
    if (request == NULL) {
        perror("strdup failed");
        return -1;
    }
    char *line = strtok(request, "\r\n");
    while (line != NULL) {
        if (strncasecmp(line, "Upgrade:", 8) == 0) {
            char *value = line + 8;
            while (*value == ' ') value++;
            if (strcasecmp(value, "websocket") == 0) {
                has_upgrade = 1;
            }
        } else if (strncasecmp(line, "Connection:", 11) == 0) {
            char *value = line + 11;
            while (*value == ' ') value++;
            if (strcasestr(value, "upgrade") != NULL) {
                has_connection_upgrade = 1;
            }
        } else if (strncasecmp(line, "Sec-WebSocket-Key:", 17) == 0) {
            char *value = line + 17;
            while (*value == ' ') value++;
            strncpy(client_key, value, WS_KEY_LENGTH);
            client_key[WS_KEY_LENGTH] = '\0';
            char *cr = strchr(client_key, '\r');
            if (cr) *cr = '\0';
            char *lf = strchr(client_key, '\n');
            if (lf) *lf = '\0';
        }
        line = strtok(NULL, "\r\n");
    }
    free(request);

    if (!has_upgrade) {
        printf("Missing or invalid Upgrade header\n");
        return -1;
    }
    if (!has_connection_upgrade) {
        printf("Missing or invalid Connection header\n");
        return -1;
    }
    if (strlen(client_key) == 0) {
        printf("Missing Sec-WebSocket-Key header\n");
        return -1;
    }
    unsigned char hash[SHA_DIGEST_LENGTH];
    
    char combined[WS_KEY_LENGTH + 38];
    strcpy(combined, client_key);

    strcat(combined, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    // SHA1((unsigned char*)combined, strlen(combined), hash);
    if (!SHA1((unsigned char*)combined, strlen(combined), hash)) {
        printf("SHA1 hash calculation failed\n");
        return -1;
    }
    char* accept_key = base64_encode(hash, SHA_DIGEST_LENGTH);
    if (accept_key == NULL) {
        printf("Base64 encoding failed\n");
        return -1;
    }
    // char response[BUFFER_SIZE];
    size_t response_len = snprintf(response, BUFFER_SIZE,
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n\r\n",
        accept_key);
        

    if (response_len >= BUFFER_SIZE) {
        printf("Response buffer overflow\n");
        free(accept_key);
        return -1;
    }
    
    // printf("response:%s",response);
    if (send(client_fd, response, strlen(response)+1, 0) < 0) {
        perror("send failed");
        free(accept_key);
        return -1;
    }
    free(accept_key);
    return 1;
}


/* WebSocket帧解析 */
int parse_ws_frame(unsigned char* buffer, ssize_t len, WebSocketFrameHeader* header, unsigned char** payload) {
    if(len < 2) return -1;

    // 解析第一个字节
    header->fin = (buffer[0] & 0x80) >> 7; //转化为0或1
    header->opcode = buffer[0] & 0x0F;

    // 打印帧头基本信息
    printf("[Frame Header]\n");
    printf("  FIN: %d (%s)\n", header->fin, header->fin ? "Final Frame" : "Fragment");
    printf("  Opcode: 0x%X (%s)\n", header->opcode, 
        header->opcode == 0x1 ? "Text Frame" :
        header->opcode == 0x2 ? "Binary Frame" :
        header->opcode == 0x8 ? "Close Frame" :
        header->opcode == 0x9 ? "Ping" :
        header->opcode == 0xA ? "Pong" : "Unknown");

    // 解析第二个字节
    header->mask = (buffer[1] & 0x80) >> 7; //提取第二个字节的最高位
    uint64_t payload_len = buffer[1] & 0x7F; //提取帧的第二个字节的低7位，表示负载长度的初始值
    
    printf("  Mask: %d (%s)\n", header->mask, 
        header->mask ? "Masked" : "Unmasked");
    printf("  Base Payload Len: %llu (0x%02X)\n", 
        (unsigned long long)payload_len, buffer[1] & 0x7F);


    int offset = 2;//表示帧头的前两个字节已经解析完成
    
    // 处理扩展长度
    if(payload_len == 126) {//表示负载长度需要额外的2字节
        if(len < 4) return -1;

        //将 buffer[2] 左移8位,与 buffer[3] 进行按位或操作，得到完整的16位负载长度。
        payload_len = (buffer[2] << 8) | buffer[3];
        offset += 2;
        printf("  Extended 16-bit Payload Len: %llu\n", 
            (unsigned long long)payload_len);
    } else if(payload_len == 127) {//表示负载长度需要额外的8字节来表示
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
        printf("  Extended 64-bit Payload Len: %llu\n",
            (unsigned long long)payload_len);
    }

    // 处理掩码
    if(header->mask) {
        if(len < offset + 4) return -1;//缓冲区中的数据不完整
        memcpy(header->masking_key, buffer + offset, 4);
        offset += 4;
        printf("  Masking Key: %02X %02X %02X %02X\n",
            header->masking_key[0],
            header->masking_key[1],
            header->masking_key[2],
            header->masking_key[3]);
    }

    // 验证负载长度
    if(len < offset + payload_len) return -1;//负载数据不完整

    // 解码负载数据
    *payload = malloc(payload_len + 1);
    //逐字节进行解码
    for(uint64_t i = 0; i < payload_len; i++) {
        (*payload)[i] = buffer[offset + i] ^ header->masking_key[i % 4];
    }
    (*payload)[payload_len] = '\0';

    // 打印负载内容
    printf("[Payload Data]\n");
    if(header->opcode == 0x1) { // 文本帧
        printf("  Text: %s\n", *payload);
    } else if(header->opcode == 0x2) { // 二进制帧
        printf("  Binary (%llu bytes):\n", (unsigned long long)payload_len);
        for(int i=0; i<payload_len; i++) {
            if(i%16 == 0) printf("    %04X: ", i);
            printf("%02X ", (*payload)[i]);
            if(i%16 == 15 || i==payload_len-1) printf("\n");
        }
    }
    printf("----------------------------------------\n");
    
    return 0;
}

/* 构造WebSocket帧 */
unsigned char* build_ws_frame(const char* message, size_t* frame_len) {
    size_t msg_len = strlen(message);
    unsigned char* frame = NULL;
    size_t header_len;

    if(msg_len <= 125) {
        header_len = 2;
        frame = malloc(header_len + msg_len);
        frame[0] = 0x81; // FIN=1, Opcode=1（文本帧）
        frame[1] = msg_len;
    } else if(msg_len <= 65535) {
        //对于2字节数据，通过逐字节操作来处理，不会受到字节序的影响。
        header_len = 4;
        frame = malloc(header_len + msg_len);
        frame[0] = 0x81;
        frame[1] = 126;
        frame[2] = (msg_len >> 8) & 0xFF;//将消息长度的低8位存储在第四个字节
        frame[3] = msg_len & 0xFF;
    } else {
        header_len = 10;
        frame = malloc(header_len + msg_len);
        frame[0] = 0x81;
        frame[1] = 127;
        //从第9个字节开始逆序存储，确保字节序正确
        for(int i = 0; i < 8; i++) {
            frame[9 - i] = (msg_len >> (8 * i)) & 0xFF;
        }
    }

    memcpy(frame + header_len, message, msg_len);
    *frame_len = header_len + msg_len;
    return frame;
}

/*处理websocket连接函数（业务函数）*/
int server_biz(int client_fd, struct sockaddr_in client_addr) {
    // 处理握手请求并返回响应
    ssize_t len = recv(client_fd, buffer, BUFFER_SIZE, 0);
    if(len<0){
        perror("recv failed\n");
        return -1;
    }
    if(len==0){
        printf("client exit\n");
        return -1;
    }
    // 执行一次握手
    if(ws_handshake(client_fd, buffer) != 1) {
        // 发送一个明确的 HTTP 错误响应
        const char* error_response = 
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n"
            "Connection: close\r\n\r\n"
            "WebSocket handshake failed";
        
        send(client_fd, error_response, strlen(error_response), 0);
        
        // 记录日志
        printf("[srv] client(%s:%d) websocket handshake failed\n", 
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        // 等待一小段时间后再关闭连接，给客户端时间接收错误信息
        usleep(100000);  // 100毫秒
        
        return -1;  // 握手失败，返回-1表示连接终止
    }

    // 握手成功，进入消息循环
    printf("handshake succedded!\n");
    while(1) {
        ssize_t len = recv(client_fd, buffer, BUFFER_SIZE, 0);
        if(len <= 0) {
            printf("[srv] client(%s:%d) disconnected!\n",
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            return -1;
        }

        WebSocketFrameHeader header;
        unsigned char* payload;
        printf("[srv] received ws frame:\n");
        if(parse_ws_frame((unsigned char*)buffer, len, &header, &payload) == 0) {
            if(header.opcode == 0x8) { // 关闭帧
                printf("client exit\n");
                return -1;
            }
            
            // 构造回声帧
            size_t frame_len;
            unsigned char* frame = build_ws_frame((char*)payload, &frame_len);
            if(!frame) {
                printf("build ws frame failed!\n");
                return -1;
            }
            
            if(send(client_fd, frame, frame_len, 0) < 0) {
                printf("send failed\n");
            }
            
            free(frame);
            free(payload);
        } else {
            printf("parse ws frame failed\n");
            return -1;
        }
    }
}

int main(int argc, char* argv[]) {
    //参数为port
    if(argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    //操作系统的所有ip都可以用于通信

    int port = atoi(argv[1]);
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd<0){
        perror("socket failed\n");
        return EXIT_FAILURE;
    }
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY
    };

    if(bind(server_fd, (struct sockaddr*)&addr, sizeof(addr))<0){
        perror("bind failed\n");
        close(server_fd);
        return EXIT_FAILURE;
    }
    if(listen(server_fd, 10)<0){
        perror("listen failed\n");
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("WebSocket server listening on port %d\n", port);

    while(1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if(client_fd<0){
            if(errno==EINTR){
                continue;
            }
            break;
        }
        printf("[srv] client(%s:%d) is accepted!\n",inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
    
        // 处理握手请求
        while(1){
            if(server_biz(client_fd,client_addr)<0){
                //客户端退出
                break;
            }
        }
        
        close(client_fd);
    }

    close(server_fd);
    return 0;
}