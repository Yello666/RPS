#include "include/config.h"
#include "include/server_biz.h"
#include "include/http_parser.h"


#define MAX 1024

void handle_client_connection(int connfd, const char *target_ip, uint16_t target_port) {
    // // 申请缓冲区用于接收套字节数据
    // char buffer[MAX];
    // // 从套接字中读取数据
    // ssize_t valread = recv(connfd,buffer,sizeof(buffer),0);
    // if (valread == 0) {
    //     printf("Client closed connection\n");
    //     return;
    // } else if (valread < 0) {
    //     perror("recv failed");
    //     return;
    // }
    // // 解析request请求
    // RequestPDU *request;
    // request=parseHttpRequest(buffer);

    // /*
    //     Todo 下面这部分应该为PDU的处理转发，这里用打印替代。
    // */
    //  // 打印解析结果
    //  printf("Method: %s\n", request.method);
    //  printf("URL: %s\n", request.url);
    //  printf("Version: %s\n", request.version);
 
    //  HeaderField *current = request.headers;
    //  while (current != NULL) {
    //      printf("Header: %s: %s\n", current->key, current->value);
    //      current = current->next;
    //  }

     /*
        Todo 从这往下只进行了转发响应，并没有进行解析，只是用来应付zx
     */
     // 连接到目标服务器
    // int target_fd = connect_to_server(target_port);
    // if (target_fd < 0) {
    //     perror("Failed to connect to target server");
    //     freeRequestPDU(&request);
    //     return;
    // }

    // // 发送请求到目标服务器
    // if (send(target_fd, buffer, valread, 0) < 0) {
    //     perror("Failed to send request to target server");
    //     safe_close(target_fd);
    //     freeRequestPDU(&request);
    //     return;
    // }

    // // 接收目标服务器的响应
    // ssize_t response_len = recv(target_fd, buffer, sizeof(buffer), 0);
    // //todo:循环接收响应
    // if (response_len < 0) {
    //     perror("Failed to receive response from target server");
    //     safe_close(target_fd);
    //     freeRequestPDU(&request);
    //     return;
    // }
    // // 解析response请求
    // ResponsePDU response = {0};
    // parseHttpResponse(buffer, &response);

    // /*
    //     Todo 下面这部分应该为PDU的处理转发，这里用打印替代。
    // */

    // // 打印解析结果
    // printf("Response from target server:\n");
    // printf("Version: %s\n", response.version);
    // printf("Status code: %s\n", response.status_code);
    // printf("Status message: %s\n", response.status_message);

    // current = response.headers;
    // while (current != NULL) {
    //     printf("Response Header: %s: %s\n", current->key, current->value);
    //     current = current->next;
    // }

    // // 这里没对响应头做处理

    // // 将响应转发回客户端
    // if (send(connfd, buffer, response_len, 0) < 0) {
    //     perror("Failed to send response to client");
    // }
 
    //  // 释放内存
    //  freeRequestPDU(&request);
}
