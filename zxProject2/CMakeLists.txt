# 设置CMake最低版本要求
cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(Reverse_Proxy_Server)

# 设置C标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 添加可执行文件
add_executable(reverse_proxy_server
        main.c
        socket_utils.c
        server_biz.c
        signal_handler.c
        http_parser.c

        ws_parser.c

)
# 查找 OpenSSL 库
find_package(OpenSSL REQUIRED)

# 包含 OpenSSL 头文件目录
target_include_directories(reverse_proxy_server PRIVATE
    ${OPENSSL_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/include
)

# 链接 OpenSSL 库
target_link_libraries(reverse_proxy_server ${OPENSSL_LIBRARIES})

# 设置包含目录
target_include_directories(reverse_proxy_server PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# 启用调试信息
set(CMAKE_BUILD_TYPE Debug)
