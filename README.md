# 高性能反向代理服务器 - C语言实现

## 📌 项目概述
一个使用纯C语言编写的轻量级反向代理服务器，支持HTTP/1.1和WebSocket协议，采用epoll多路复用实现高并发连接处理。本项目从底层手动实现协议解析和网络通信，旨在深入理解网络编程的核心原理。

## 🎯 核心特性
- ✅ **HTTP/1.1反向代理** - 支持GET、POST等方法，保持Keep-Alive连接
- ✅ **WebSocket协议支持** - 完整实现RFC6455，支持帧分片和Ping/Pong心跳
- ✅ **高并发模型** - 基于epoll ET模式的事件驱动架构
- ✅ **协议手动解析** - 不依赖第三方库，完全自主实现HTTP/WebSocket协议栈
- ✅ **连接管理** - 支持连接池、超时控制和优雅关闭
- ✅ **配置灵活** - 支持路由规则配置和负载均衡策略

## 📊 性能表现
- 单进程支持 **3000+** 并发WebSocket连接
- 数据转发延迟 **< 5ms**（局域网环境）
- 内存占用稳定，无内存泄漏
- 支持长时间稳定运行

## 🏗️ 系统架构

```
客户端请求
     ↓
[监听端口:80/443]
     ↓
[主线程 epoll_wait] ←→ [事件就绪队列]
     ↓
[连接处理器]
 ├── HTTP请求 → [HTTP解析器] → [路由匹配] → [转发至后端]
 └── WebSocket握手 → [WS握手处理器] → [双工转发通道]
     ↓
[后端服务器] ←→ [数据双向转发] ←→ [客户端]
```

### 核心模块设计
1. **事件循环引擎** - 单Reactor模式，所有I/O在同一epoll实例中处理
2. **协议解析器** - 状态机驱动的流式解析，支持不完整数据包缓存
3. **连接上下文** - 统一管理HTTP/WebSocket连接状态
4. **缓冲区管理** - 零拷贝数据转发优化
5. **监控统计** - 实时连接数、流量统计

## 🔧 技术栈
- **语言**: C
- **系统调用**: epoll (ET模式)、socket、fcntl (非阻塞I/O)
- **协议实现**: HTTP/1.1 (部分)、WebSocket 
- **工具链**: GCC、Make、GDB、zlog
- **测试工具**:apifox、curl

## 📁 项目结构
```
.
├── CMakeLists.txt              #cmakelist
├── RPS_logs                    #日志文件
├── build
│   └── rps                     #可执行文件
├── cJSON                       #json解析库
├── clean_and_rebuild.sh        #编译脚本文件
├── config                      #配置文件
│   └── config.json
├── connection_manager.c        #连接池实现
├── http_parser.c               #HTTP请求解析实现
├── include                     #头文件
├── log.c                       #日志功能实现
├── main.c                      #主函数
├── server_biz.c                #初始化及一些功能函数
├── signal_handler.c            #信号处理器
├── socket_utils.c              #解析url实现
├── ws_parser.c                 #websocket协议解析实现
├── zlog                        #日志库
```

## 🚀 快速开始
### 配置代理
编辑 `config.json`:
```
{
  "Http": {
    "/test":"192.168.64.1:8083"
    //路由：转发目的服务器地址
  },
  "Websocket": {
    "/v1/ws/0009":"192.168.64.1:8000",    
  },
```

### 编译项目
```bash
./clean_and_rebuild.sh
```
### 启动服务器
```bash
# 启动反向代理
./build/rps

# 测试HTTP代理
需要在config.json中配置路由重定向规则
curl -x http://localhost:8080 http://backend-server/api/test

# 测试WebSocket
# 使用wscat或自定义客户端连接
```

## 🔍 协议实现细节

### HTTP解析器特点
- 基于状态机的流式解析
- 支持Chunked传输编码
- 自动处理Keep-Alive连接
- 请求头大小限制防护

### WebSocket实现要点
```c
// 握手响应示例
HTTP/1.1 101 Switching Protocols
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Accept: {calculated_key}

// 数据帧格式
// 支持类型：文本(0x1)、二进制(0x2)、关闭(0x8)、Ping(0x9)、Pong(0xA)
```

### 帧处理流程
```
接收原始数据 → 解析帧头 → 计算载荷长度 → 处理掩码 → 分片重组 → 应用处理
```

## ⚡ 性能优化

### 关键技术点
1. **水平触发(LT)模式** - 减少丢事件的发生
2. **非阻塞I/O** - 避免线程阻塞，提高吞吐量
3. **连接池** - 连接对象和缓冲区的预分配重用
4. **零拷贝转发** - 同一连接内数据直接转发
5. **事件批处理** - 单次epoll_wait处理多个就绪事件

### 内存管理策略
- 每个连接固定大小的接收/发送缓冲区
- 定时清理空闲连接释放资源
