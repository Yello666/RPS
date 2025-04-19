#include "include/config.h"
#include "include/socket_util.h"

// 根据ip port创建socket并监听
int create_ipv4_server(const char *ip, uint16_t port) {
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("socket creation failed");
        return SERVER_SOCKET_ERROR;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = inet_addr(ip)
    };

    if (addr.sin_addr.s_addr == INADDR_NONE) {
        perror("invalid IP address");
        close(listenfd);
        return -1;
    }

    // 设置套接字为可复用
    int opt = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(listenfd);
        return SERVER_SETSOCKOPT_ERROR;
    }

    if (bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        close(listenfd);
        return SERVER_BIND_ERROR;
    }

    if (listen(listenfd, SOMAXCONN) < 0) {
        perror("listen failed");
        close(listenfd);
        return SERVER_LISTEN_ERROR;
    }

    return listenfd;
}

// safe_close 安全关闭套接字连接
void safe_close(int sockfd) {
    if (sockfd >= 0) {
        if (close(sockfd) < 0) {
            perror("close socket failed");
        }
    }
}

// 连接到目标服务器
int connect_to_server(uint16_t port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        return -1;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = inet_addr(DEFAULT_IP)
    };

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect failed");
        close(sockfd);
        return -1;
    }

    return sockfd;
}