#ifndef SERVER_BIZ_H
#define SERVER_BIZ_H

// 处理客户端连接
void handle_client_connection(int connfd, const char *target_ip, uint16_t target_port);


#endif