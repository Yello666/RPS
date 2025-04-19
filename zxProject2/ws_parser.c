#include "include/ws_parser.h"
#include "include/http_parser.h"
#include "include/log.h"
#include "include/config.h" //定义了BUFFER_SIZE
#include <unistd.h>
#include <errno.h>
// #include <netinet/in.h>

// #define BUFFER_SIZE 4096
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9090
#define WS_KEY_LENGTH 24
//转发到后端服务器的请求头
char upstream_request[BUFFER_SIZE];
//接收后端服务器响应
char backend_response[BUFFER_SIZE];
//连接建立后进行数据帧收发的缓冲区
char wsbuffer[BUFFER_SIZE];
//储存sec-websocket-key
char client_key[WS_KEY_LENGTH + 1];
/*
TODO:
add_connection();//记录与客户端和服务端的连接
remove_connection();//删除与客户端或服务端的连接
*/

/*处理websocket请求*/
int ws_handler(int client_sock, struct sockaddr_in client_addr, RequestPDU* request) {
    
    // 1. 记录与客户端的连接
    LOG("New WebSocket connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    // add_connection(client_sock,client_addr.sin_addr,client_addr.sin_port);//todo

    //2.解析http请求头，并判断请求是否合法
    if(parse_header(client_sock,request)<0){
        // 发送一个明确的 HTTP 错误响应
        const char* error_response = 
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n"
            "Connection: close\r\n\r\n"
            "WebSocket handshake failed";
        
        if(send(client_sock, error_response, strlen(error_response), 0)<0){
            LOG("send failed\n");
            return -1;
        }
        
        // 记录日志
        LOG("[srv] client(%s:%d) websocket handshake failed\n", 
                inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        // 等待一小段时间后再关闭连接，给客户端时间接收错误信息
        usleep(100000);  // 100毫秒
        
        return -1;  // 握手失败，返回-1表示连接终止
        
    }

    // 3. 构造转发到后端服务器的请求头
    // char upstream_request[4096];
    //X-Forwarded-Proto （记录曾经使用的协议，用于转发url时决定是使用ws还是wss）
    //暂定不会使用wss，所以直接写成http
    char host[50];
    sprintf(host,"%s:%d",SERVER_IP,SERVER_PORT);
    snprintf(upstream_request, sizeof(upstream_request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: Upgrade\r\n"
        "Upgrade: websocket\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Key: %s\r\n" 
        "X-Real-IP: %s \r\n"
        "X-Forwarded-Proto : http \r\n" 
        "\r\n",
        request->url,
        host,
        client_key,inet_ntoa(client_addr.sin_addr));
    // 4. 连接后端服务器
    int backend_sock = connect_to_backend(); 
    if(backend_sock<0){
        LOG("[rps] connect backend server failed\n");
        return -1;
    }
    // add_connection(backend_sock,SERVER_IP,SERVER_PORT);

    // 5. 转发升级请求到后端
    if (send(backend_sock, upstream_request, strlen(upstream_request), 0) < 0) {
        LOG("[rps] Backend send error: %s\n", strerror(errno));
        close(backend_sock);
        return -1;
    }

    // 6. 接收后端响应
    // char backend_response[BUFFER_SIZE];
    ssize_t len = recv(backend_sock, backend_response, BUFFER_SIZE, 0);
    if (len <= 0) {
        LOG("[rps] recv backend response error\n");
        close(backend_sock);
        return -1;
    }

    // 7. 检查是否为101 Switching Protocols
    if (strstr(backend_response, "101 Switching Protocols") == NULL) {
        LOG("[rps]Backend refused WebSocket upgrade\n");
        close(backend_sock);
        return -1;
    }
    LOG("[rps] server(%s:%d) websocket handshake succeeded!\n",SERVER_IP,SERVER_PORT);
    
    // 8. 转发响应给客户端
    if (send(client_sock, backend_response, len, 0) < 0) {
        LOG("Client send error: %s\n", strerror(errno));
        close(backend_sock);
        return -1;
    }
    LOG("[rps] client(%s:%d) websocket handshake succeeded!\n",inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
    // 9. 简单循环收发（阻塞式）
    // char wsbuffer[BUFFER_SIZE];
    while (1) {
        // 客户端 -> RPS
        ssize_t rcvlen = recv(client_sock, wsbuffer, BUFFER_SIZE, 0);
        if (rcvlen == 0) {
            LOG("[rps] Client exit\n");
            break;
        } 
        if(rcvlen<0){
            LOG("[rps] recv from client failed\n");
            break;
        }
        //检查数据帧中的opcode是否为关闭帧，如果是的话，向服务端关闭连接
        WebSocketFrameHeader* header;
        header=(WebSocketFrameHeader*)malloc(sizeof(WebSocketFrameHeader));
        unsigned char* payload=NULL;
        LOG("[rps] received client websocket frame:\n");
        if(parse_ws_frame((unsigned char*)wsbuffer,rcvlen,header,&payload)<0){
            LOG("[rps] parse ws frame failed\n");
            free(payload);
            free(header);
            continue;
        }
        if(header->opcode==0x8){
            LOG("[rps] client closed websocket connection\n");
            memset(wsbuffer,0,BUFFER_SIZE);
            // uint16_t code = parse_close_code((uint8_t*)wsbuffer + 2, rcvlen - 2);
            uint16_t code = 1000; // 默认正常关闭代码
            if(payload != NULL && header->payload_len>= 2){
                code = ((payload[0]) << 8) | (payload[1]); // 解析关闭代码
            }
            send_close_frame(backend_sock, code);  // 转发 Close 到后端
            safe_ws_close(client_sock,backend_sock);
            // remove_connection();//todo
            return -1;
        }

        //RPS->后端
        if (send(backend_sock, wsbuffer, rcvlen, 0) < 0) {
            LOG("Backend forward error\n");
            break;
        }

        // 后端 -> RPS
        rcvlen = recv(backend_sock,wsbuffer, BUFFER_SIZE, 0);
        if (rcvlen <= 0) {
            LOG("Backend disconnected\n");
            break;
        }
        LOG("[rps] received server websocket frame:\n");
        
        unsigned char* server_payload=NULL;
        WebSocketFrameHeader *server_header;
        server_header=(WebSocketFrameHeader*)malloc(sizeof(WebSocketFrameHeader));
        if(parse_ws_frame((unsigned char*)wsbuffer,rcvlen,server_header,&server_payload)<0){
            LOG("[rps] parse ws frame failed\n");
            free(server_payload);
            free(server_header);
            break;
        }
        //RPS->客户端
        if (send(client_sock, wsbuffer, rcvlen, 0) < 0) {
            LOG("Client forward error\n");
            break;
        }
        free(payload);
        free(header);
        free(server_payload);
        free(server_header);
    }

    // 9. 清理
    LOG("WebSocket connection closed\n");
    close(backend_sock);
    // remove_connection();//todo
    return 1;
}

/*解析http请求头并判断是否合法*/
int parse_header(int client_sock,RequestPDU* request){
    int has_upgrade = 0;
    int has_connection_upgrade = 0;
    // char client_key[WS_KEY_LENGTH + 1] = {0};

    // 从请求结构体中提取必要的头字段
    HeaderField *current = request->headers;
    while (current != NULL) {
        if (strcasecmp(current->key, "Upgrade") == 0) {
            if (strcasecmp(current->value, "websocket") == 0) {
                has_upgrade = 1;
            }
        } else if (strcasecmp(current->key, "Connection") == 0) {
            if (strcasestr(current->value, "upgrade") != NULL) {
                has_connection_upgrade = 1;
            }
        } else if (strcasecmp(current->key, "Sec-WebSocket-Key") == 0) {
            strncpy(client_key, current->value, WS_KEY_LENGTH);
            client_key[WS_KEY_LENGTH] = '\0';
        }
        current = current->next;
    }

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
    printf("legal header!\n");
    return 1;

}

/* WebSocket帧解析 调用时需注意释放payload的内存*/
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
        header->payload_len=payload_len;
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

/* 构造WebSocket帧 调用时需注意释放frame的内存*/
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

/*与后端服务器进行连接*/
int connect_to_backend(){
    // 创建socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd<0){
        LOG("[rps] backend socket failed\n");
        return -1;
    }
    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT)
    };
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    // 连接服务器
    if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        LOG("[rps] backend connect failed\n");
        close(sockfd);
        return -1;
    }
    LOG("[rps] connected to server(%s:%d)!\n",SERVER_IP,SERVER_PORT);
    return sockfd;
}


/*安全关闭 WebSocket 连接（双向握手）*/
void safe_ws_close(int client_sock, int backend_sock) {
    char buffer[128];//临时缓冲区
    // 1. 向两端发送 Close 帧（如果连接仍活跃）
    shutdown(client_sock, SHUT_WR);  // 半关闭，防止后续写入
    shutdown(backend_sock, SHUT_WR);

    // 2. 接收剩余数据（可选，避免 RST)
    while (recv(client_sock, buffer, sizeof(buffer), 0) > 0);
    while (recv(backend_sock, buffer, sizeof(buffer), 0) > 0);

    // 3. 完全关闭
    close(backend_sock);
    //client_sock在main函数的循环中关闭
}

// 构造 Close 帧（含状态码）
void send_close_frame(int sock, uint16_t code ) { 
    uint8_t close_frame[4] = {
        0x88, 0x02,  // FIN=1, opcode=8, 长度=2
        (uint8_t)(code >> 8), (uint8_t)(code & 0xFF)  // 大端序状态码
    };
    send(sock, close_frame, sizeof(close_frame), 0);
}