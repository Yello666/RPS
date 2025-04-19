#ifndef SOCKET_UTIL_H
#define SOCKET_UTIL_H

// 创建IPv4服务器套接字
int create_ipv4_server(const char *ip, uint16_t port);

// 安全关闭套接字
void safe_close(int sockfd);

// 连接到目标服务器
int connect_to_server(uint16_t port);
#endif