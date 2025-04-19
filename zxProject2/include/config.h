#ifndef CONFIG_H
#define CONFIG_H

/*
    依赖部分
*/
// 原生库依赖
#include <stdio.h>
#include <signal.h> // 信号相关
#include <string.h>
#include <sys/socket.h> // socket相关
#include <sys/types.h>
#include <netinet/in.h> // 网络地址相关
#include <arpa/inet.h> // ip地址转换
#include <unistd.h> // 系统调用
#include <stdlib.h>
#include <stdint.h>
#include <errno.h> // 检查错误
#include <ctype.h> // 判断字符串是否为大小写
/*
    常量部分
*/

// 定义默认的IP地址
#define DEFAULT_IP "0.0.0.0"

// 定义默认的端口号
#define DEFAULT_PORT 8080
// 定义默认远程端口号
#define DEFAULT_REMOTE_PORT 8083

// 定义监听队列的长度
#define BACKLOG 10
// 定义buffer的大小
#define BUFFER_SIZE 8192
// 定义最大的请求头大小
#define MAX_HEADER_SIZE 8192

#define CONFIG_FILE = "../config/config.yml"

// 定义错误码
#define SERVER_SOCKET_ERROR -1 // 创建服务器套接字时发生错误
#define SERVER_SETSOCKOPT_ERROR -2 // 设置服务器套接字选项时发生错误
#define SERVER_BIND_ERROR -3 // 服务器套接字绑定地址和端口时发生错误
#define SERVER_LISTEN_ERROR -4 // 服务器套接字开始监听时发生错误
#define CLIENT_SOCKET_ERROR -5 // 创建客户端套接字时发生错误
#define CLIENT_RESOLVE_ERROR -6 // 客户端解析地址时发生错误
#define CLIENT_CONNECT_ERROR -7 // 客户端连接服务器时发生错误
#define CREATE_PIPE_ERROR -8 // 创建管道时发生错误
#define BROKEN_PIPE_ERROR -9 // 管道破裂错误（当向一个没有读端的管道写入数据时，会触发 SIGPIPE 信号，并且可以用这个错误码来表示这种情况）
#define HEADER_BUFFER_FULL -10 // ：HTTP 头部缓冲区已满。在处理 HTTP 请求或响应时，如果存储头部信息的缓冲区已满，无法再存储更多信息，就使用该错误码
#define BAD_HTTP_PROTOCOL -11 // HTTP 协议错误。当接收到的 HTTP 请求或响应不符合 HTTP 协议规范时，使用该错误码来标识。
#define SERVER_MALLOC_ERROR -12 // 分配内存失败错误码
#define FIELD_NOT_EXIST -13 // 字段未存在

#endif // CONFIG_H
