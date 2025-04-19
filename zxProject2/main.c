#include "include/config.h"
#include "include/log.h"
#include "include/socket_util.h"
#include "include/signal_handler.h"
#include "include/http_parser.h"
#include "include/ws_parser.h"
int remote_port;
int local_port;

int server_sock;
int client_sock;
int remote_sock;

char headBuffer[MAX_HEADER_SIZE];

// 服务器主循环
void server_loop();
// 处理连接
void handle_client(int client_sock, struct sockaddr_in client_addr);

// 读取配置文件匹配port
// int get_port_from_config();

int main() {
	// 设置本地监听端口和远程端口
    local_port = DEFAULT_PORT;
    remote_port = DEFAULT_REMOTE_PORT;

    // 初始化信号处理器，只捕获sigint信号目前
    // sigaction_init();

    // 初始化socket并bind启动服务
    server_sock = create_ipv4_server(DEFAULT_IP,local_port);
    if (server_sock < 0) {
        LOG("[srv] Server start failed\n");
        exit(EXIT_FAILURE);
    }

    LOG("[srv] Server started on %s:%d\n", DEFAULT_IP, local_port);

    server_loop();

    // 收发大循环，单进程启动
    // while (!sigint_flag) {
    //     struct sockaddr_in client_addr;
    //     socklen_t addrlen = sizeof(client_addr);
    //
    //     int connfd = accept(listenfd, (struct sockaddr*)&client_addr, &addrlen);
    //     if (connfd < 0) {
    //         if (errno == EINTR) continue;
    //         perror("accept failed");
    //         break;
    //     }
    //
    //     char client_ip[INET_ADDRSTRLEN];
    //     inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    //     printf("[srv] Client connected: %s:%d\n",
    //          client_ip, ntohs(client_addr.sin_port));
    //
    //     // 处理收到的业务数据，并转发请求到目标服务器
    //     handle_client_connection(connfd, target_ip, target_port);
    //     // 关闭本次连接
    //     safe_close(connfd);
    //     printf("[srv] Client Closed: %s:%d\n",
    //         client_ip, ntohs(client_addr.sin_port));
    // }

    // 服务结束关闭socket
    safe_close(server_sock);
    LOG("[srv] Server shutdown complete\n");
    return 0;
}

/*
 * 服务端主循环
 * 1. 等待客户端连接
 * 2. 处理客户端请求
 */
void server_loop() {
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);

    while (!sigint_flag) {
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addrlen);
        if (client_sock < 0) {
            if (errno == EINTR) continue;
            perror("accept failed");
            break;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        LOG("[srv] Client connected: %s:%d\n",
             client_ip, ntohs(client_addr.sin_port));

        // todo 创建子进程处理请求
        handle_client(client_sock, client_addr);

        safe_close(client_sock);
        LOG("[srv] Client closed: %s:%d\n",
             client_ip, ntohs(client_addr.sin_port));
    }

}

/*
 * 处理客户端请求
 */
void handle_client(int client_sock, struct sockaddr_in client_addr) {
    int flag = 1;
    while(flag){
        // 从socket中读取数据缓存到headBuffer中,读取到连续的/n就停止
        int i = 0;
        while(1){
            ssize_t valread = recv(client_sock,headBuffer+i,1,0);
            if (valread == 0) {
                printf("Client closed connection\n");
                return;
            }
            if (valread < 0) {
                perror("recv failed");
                return;
            }
            // 读到/n就继续往后读两个字节，如果还是/n就停止
            if (headBuffer[i] == '\n'){
                ssize_t valread = recv(client_sock,headBuffer+i+1,2,0);
                if (valread == 0) {
                    printf("Client closed connection\n");
                    break;
                }
                if (valread < 0) {
                    perror("recv failed");
                    return;
                }
                i+=2;
                if (headBuffer[i]=='\n'){
                    break;
                }
            }
            i+=1; // 如果不是则继续读
        }

        // 将headBuffer中数据解析成requestPDU的形式便于处理
        RequestPDU *request = parseHttpRequest(headBuffer);
        if (request == NULL){
            return;
        }

        // 查询是否为keep-alive，如果是则flag置为1，不是则置为零
        HeaderField *connection = search_header_field(request->headers,"Connection");
        if (strcmp(connection->value,"close")==0){
            flag=0;
        }

//**************************************************************************** */
        // 查询字段用于判断是http还是websocket,如果search_header_field之后确定是webs升级请求，那么就转webs业务函数
        // 后面修改请求头、读payload(无)不需要
        if(strcasecmp(connection->value,"upgrade")==0){
            //处理websocket请求
            if(ws_handler(client_sock,client_addr,request)<0){
                //ws连接服务关闭
                LOG("[rps] websocket connection closed\n");
                // safe_close(client_sock);//外层会关闭
                return;
            }
        }

        // // 修改请求头(http和websocket不能共用，websocket需要单独实现)
        // // 修改请求头，判断是否有错误
        // int err = handle_request_header(request);
        // if (err!=0){
        //     LOG("ERROR:修改请求头失败\n");
        //     return;
        // }

        // // 读取payload
        // // 1.读取字段大小 2.循环读取
        // HeaderField *length = search_header_field(request->headers,"Content-Length");
        // // 如果该字段不存在,则查询小写 
        // char *payload=NULL;
        // if (length!=NULL){
        //     int value = atoi(length->value);
        //     payload = read_payload(client_sock,value);
        // }

        // // 打印解析结果
        // printf("Method: %s\n", request->method);
        // printf("URL: %s\n", request->url);
        // printf("Version: %s\n", request->version);
        // HeaderField *current = request->headers;
        // while (current != NULL) {
        //     printf("Header: %s: %s\n", current->key, current->value);
        //     current = current->next;
        // }
        // printf("Payload: %s\n",payload);
    
   
    


        // TODO 识别请求头中的url，读取配置文件获取地址
        // remote_port = get_port_from_config(request->url);
        // if (remote_port == -1){
        //     LOG("ERROR: get_port_from_config failed\n");
        //     return;
        // }
        // 建立连接
        // remote_sock = connect_to_server(remote_port);
        // if (remote_sock < 0) {
        //     LOG("connect to server fail");
        //     return;
        // }

        // // 发送数据
        // // 构造发送的数据
        // char *send_buffer = requestToBuffer(request,payload);
        // if (send_buffer == NULL) {
        //     LOG("ERROR: requestToBuffer failed\n");
        //     close(remote_sock);
        //     return;
        // }

        // // 计算实际要发送的总长度
        // size_t total_length = strlen(send_buffer);

        // ssize_t sent_bytes = send(remote_sock, send_buffer, total_length, 0);
        // if (sent_bytes < 0) {
        //     perror("send failed");
        // } else if ((size_t)sent_bytes != total_length) {
        //     LOG("ERROR: Incomplete data sent\n");
        // }

        // // 读取响应头
        // // 清除headBuffer
        // memset(headBuffer, 0, sizeof(headBuffer));
        // // 从socket中读取数据缓存到headBuffer中,读取到连续的/n就停止
        // i = 0;
        // while(1){
        //     ssize_t valread = recv(remote_sock,headBuffer+i,1,0);
        //     if (valread == 0) {
        //         printf("Client closed connection\n");
        //         break;
        //     }
        //     if (valread < 0) {
        //         perror("recv failed");
        //         return;
        //     }
        //     // 读到/n就继续往后读两个字节，如果还是/n就停止
        //     if (headBuffer[i] == '\n'){
        //         ssize_t valread = recv(remote_sock,headBuffer+i+1,2,0);
        //         if (valread == 0) {
        //             printf("Client closed connection\n");
        //             break;
        //         }
        //         if (valread < 0) {
        //             perror("recv failed");
        //             return;
        //         }
        //         i+=2;
        //         if (headBuffer[i]=='\n'){
        //             break;
        //         }
        //         }
        //     i+=1; // 如果不是则继续读
        // }
        // // 将headBuffer中数据解析成responsePDU的形式便于处理
        // ResponsePDU *response = parseHttpResponse(headBuffer);
        // if (response == NULL){
        //     return;
        // }
        // // 修改响应头字段
        // err=handle_response_header(response,flag);
        // if (err!=0){
        //     LOG("ERROR: 修改请求头出错\n");
        //     break;
        // }
        
    
        // // 读取响应体
        // // 查询是否content-length字段，没有则不读取
        // length = search_header_field(response->headers,"Content-Length");
        // if (length!=NULL){
        //     int value = atoi(length->value);
        //     payload = read_payload(client_sock,value);
        // }

        
        // // 打印解析结果
        // printf("Version: %s\n", response->version);
        // printf("Status_Code: %s\n", response->status_code);
        // printf("Status_Message: %s\n", response->status_message);
        // current = response->headers;
        // while (current != NULL) {
        //     printf("Header: %s: %s\n", current->key, current->value);
        //     current = current->next;
        // }
        // printf("Payload: %s\n",payload);

        // // 转发响应到客户端
        // // 构造响应数据
        // send_buffer = requestToBuffer(response,payload);
        // if (send_buffer == NULL) {
        //     LOG("ERROR: requestToBuffer failed\n");
        //     close(remote_sock);
        //     return;
        // }

        // // 计算实际要发送的总长度
        // total_length = strlen(send_buffer);

        // sent_bytes = send(client_sock, send_buffer, total_length, 0);
        // if (sent_bytes < 0) {
        //     perror("send failed");
        // } else if ((size_t)sent_bytes!= total_length) {
        //     LOG("ERROR: Incomplete data sent\n");
        // }
    
        // // 释放send_buffer
        // free(send_buffer);

        // 关闭连接
        safe_close(remote_sock);
        // TODO 是否需要关闭client_sock 
        
        // // 释放内存
        // freeResponsePDU(response);
        // freeRequestPDU(request);
    }
}

// // 读取配置文件匹配port
// int get_port_from_config(char* url) {
//     return DEFAULT_REMOTE_PORT;
// }