// http_parser.c
#include "include/config.h"
#include "include/http_parser.h"
#include "include/log.h"
#define MAX 1024 //接收服务器的状态行

// 创建新的头字段节点
HeaderField* createHeaderField(const char *key, const char *value) {
    HeaderField *newField = (HeaderField*)malloc(sizeof(HeaderField));
    if (newField == NULL) {
        perror("Memory allocation failed");
        return NULL;
    }

     // 复制原始的key
     char *keyCopy = strdup(key);
     if (keyCopy == NULL) {
         perror("strdup failed");
         free(newField);
         return NULL;
     }
 
     // 转换key为标准形式，每个连字符后的首字母大写
     int capitalize = 1;
     for (int i = 0; keyCopy[i] != '\0'; i++) {
         if (capitalize && islower((unsigned char)keyCopy[i])) {
             keyCopy[i] = (char)toupper((unsigned char)keyCopy[i]);
         }
         capitalize = (keyCopy[i] == '-');
     }
 
    newField->key = keyCopy;
    newField->value = strdup(value);
    newField->next = NULL;
    return newField;
}

// 解析请求行 
int parseRequestLine(const char *line, RequestPDU *request) {
    char *token = strtok((char*)line, " ");
    if (token != NULL) {
        request->method = strdup(token);
        if (request->method == NULL) {
            perror("strdup failed");
            return BAD_HTTP_PROTOCOL;
        }
    }

    token = strtok(NULL, " ");
    if (token != NULL) {
        request->url = strdup(token);
        if (request->url == NULL) {
            perror("strdup failed");
            free(request->method);
            return BAD_HTTP_PROTOCOL;
        }
    }

    token = strtok(NULL, " ");
    if (token != NULL) {
        request->version = strdup(token);
        if (request->version == NULL) {
            perror("strdup failed");
            free(request->method);
            free(request->url);
            return BAD_HTTP_PROTOCOL;
        }
    }

    return 0;
}

// 解析头字段
HeaderField *parseHeaders(const char *headers) {
    char *line = strdup(headers);
    if (line == NULL) {
        perror("strdup failed");
        return NULL;
    }
    char *saveptr;
    char *token = strtok_r(line, "\r\n", &saveptr);

    // 申请新结点
    HeaderField *root = createHeaderField("","");

    while (token != NULL) {
        char *colon = strchr(token, ':');
        if (colon != NULL) {
            *colon = '\0';
            char *key = token;
            char *value = colon + 1;
            // 去除值前面的空格
            while (*value == ' ') value++;

            HeaderField *newField = createHeaderField(key, value);
            if (newField != NULL) {
                newField->next = root->next;
                root->next = newField;
            }
        }
        token = strtok_r(NULL, "\r\n", &saveptr);
    }
    free(line);
    HeaderField *current = root->next;
    free(root);
    return current;
}

// 解析HTTP请求
RequestPDU *parseHttpRequest(const char *data) {
    // 初始化Request
    RequestPDU *request;
    request = (RequestPDU *)malloc(sizeof(RequestPDU));
    if (request == NULL){
        LOG("ERROR:申请request内存失败!\n");
        return NULL;
    }

    char *copy = strdup(data);
    // 解析请求行（使用 strstr 避免破坏后续解析）
    char *requestLineEnd = strstr(copy, "\r\n");
    if (!requestLineEnd) {
        free(copy);
        return NULL;  // 无请求行终止符
    }
    *requestLineEnd = '\0';  // 截断请求行
    char *requestLine = copy;
    int error = parseRequestLine(requestLine, request);
    if (error != 0) {
        LOG("ERROR:解析http请求行错误\n");
        return NULL;
    }

    // 解析头部（直接跳过已找到的 "\r\n"）
    char *headers = requestLineEnd + 2;
    if (*headers == '\0') {
        headers = NULL;  // 无头部
    }
    request->headers=parseHeaders(headers);
    if (request->headers==NULL){
        LOG("ERROR:解析http请求头字段出错\n");
        free(copy);
        return NULL;
    }

    free(copy);
    return request;
}

 // 释放请求PDU的内存
void freeRequestPDU(RequestPDU *request) {
    free(request->method);
    free(request->url);
    free(request->version);

    HeaderField *current = request->headers;
    while (current != NULL) {
        HeaderField *next = current->next;
        free(current->key);
        free(current->value);
        free(current);
        current = next;
    }
    LOG("INFO: free requestPDU!\n");
}

// // 辅助函数：解析一行数据，去掉\r
// const char *parse_line(const char *ptr, char *line, size_t max_len) {
//     char *end = (char *)memchr(ptr, '\n', max_len);
//     if (end) {
//         size_t len = end - ptr;
//         if (len > 0 && ptr[len - 1] == '\r') {
//             len--; // 去掉'\r'
//         }
//         if (len > max_len - 1) len = max_len - 1; //超过缓冲区，截断
//         memcpy(line, ptr, len);
//         line[len] = '\0';
//         return end + 1; // 跳过'\n'
//     }
//     return ptr + strlen(ptr); // 如果没有找到'\n'，返回末尾
// }

// 解析状态行
int parseResponseLine(const char *line, ResponsePDU *response) {
    char *token = strtok((char*)line, " ");
    if (token != NULL) {
        response->version = strdup(token);
        if (response->version == NULL) {
            perror("strdup failed");
            return BAD_HTTP_PROTOCOL;
        }
    }

    token = strtok(NULL, " ");
    if (token != NULL) {
        response->status_code = strdup(token);
        if (response->status_code == NULL) {
            perror("strdup failed");
            free(response->version);
            return BAD_HTTP_PROTOCOL;
        }
    }

    token = strtok(NULL, " ");
    if (token != NULL) {
       response->status_message = strdup(token);
        if (response->status_message == NULL) {
            perror("strdup failed");
            free(response->version);
            free(response->status_code);
            return BAD_HTTP_PROTOCOL;
        }
    }

    return 0;
}

// // 添加头字段到链表
// void add_header_field(char *key, char *value, HeaderField **headers) {
//     HeaderField *new_field = (HeaderField *)malloc(sizeof(HeaderField));
//     new_field->key = key;
//     new_field->value = value;
//     new_field->next = NULL;

//     if (*headers == NULL) {
//         *headers = new_field;
//     } else {
//         HeaderField *current = *headers;
//         while (current->next != NULL) {
//             current = current->next;
//         }
//         current->next = new_field;
//     }
// }

// // 解析头字段
// void parse_header_line(const char *header_line, HeaderField **headers) {
//     char *line = strdup(header_line);
//     if (!line) return;
    
//     char *key = NULL;
//     char *value = NULL;
//     char *colon = strchr(line, ':');
//     if (colon) {
//         *colon = '\0';
//         key = strdup(line);
//         value = strdup(colon + 1);
//         // 去除值前面的空格
//         while (*value == ' ') value++; 
//     }
//     add_header_field(key, value, headers);
//     free(line);
// }

// 解析HTTP响应
ResponsePDU *parseHttpResponse(const char *data) {
    // 初始化响应PDU
    ResponsePDU *response;
    response = (ResponsePDU *)malloc(sizeof(ResponsePDU));
    if (response == NULL){
        LOG("ERROR:申请response内存失败!\n");
        return NULL;
    }

    char *copy = strdup(data);
    // 解析请求行（使用 strstr 避免破坏后续解析）
    char *responseLineEnd = strstr(copy, "\r\n");
    if (!responseLineEnd) {
        free(copy);
        return NULL;  // 无请求行终止符
    }
    *responseLineEnd = '\0';  // 截断请求行
    char *responseLine = copy;
    int error = parseResponseLine(responseLine, response);
    if (error != 0) {
        LOG("ERROR:解析http响应行错误\n");
        return NULL;
    }
    // 解析头部（直接跳过已找到的 "\r\n"）
    char *headers = responseLineEnd + 2;
    if (*headers == '\0') {
        headers = NULL;  // 无头部
    }
    response->headers=parseHeaders(headers);
    if (response->headers==NULL){
        LOG("ERROR:解析http响应头头字段出错\n");
        free(copy);
        return NULL;
    }

    free(copy);
    return response;
}


// // 释放头字段链表的内存
// void free_header_fields(HeaderField *headers) {
//     HeaderField *current = headers;
//     while (current != NULL) {
//         HeaderField *temp = current;
//         current = current->next;
//         free(temp->key);
//         free(temp->value);
//         free(temp);
//     }
// }

// 释放响应PDU的内存
void freeResponsePDU(ResponsePDU *response) {
    free(response->version);
    free(response->status_code);
    free(response->status_message);
    HeaderField *current = response->headers;
    while (current != NULL) {
        HeaderField *next = current->next;
        free(current->key);
        free(current->value);
        free(current);
        current = next;
    }
    LOG("INFO: free responsePDU!\n");
}

// search_header_field 搜索HTTP的header中的特定字段
HeaderField *search_header_field(HeaderField *current,char *key){
    // 遍历http请求头，找到特定字段对应指针
    while(current!=NULL){
        if (strcmp(current->key,key)==0){
            return current;
        }
        current=current->next;
    }
    return NULL;
}

// handle_request_header 处理http请求头字段，目前只处理host字段和connect字段
int handle_request_header(RequestPDU *request)
{   
    // 找到Host字段，修改其为目标服务器地址
    HeaderField *host = search_header_field(request->headers,"Host");
    if (host==NULL){
        return FIELD_NOT_EXIST;
    }
    char temp[20];

    sprintf(temp,"%s:%d",DEFAULT_IP,DEFAULT_REMOTE_PORT);
    free(host->value);
    host->value=strdup(temp);
    // 修改Connection字段改为close
    HeaderField *connection = search_header_field(request->headers,"Connection");
    if (connection==NULL){
        return FIELD_NOT_EXIST;
    }

    sprintf(temp,"%s","close");
    free(connection->value);
    connection->value=strdup(temp);

    return 0;
}

// read_payload 读取payload数据
char * read_payload(int client_sock,int length){
    char small_buffer[1024];// 小缓冲区
    char *large_buffer = NULL;// 大缓存区
    size_t total_received = 0;// 已接收的数据的总长度
    int n;

    // 为大缓冲区分配初始内存
    large_buffer = (char *)malloc(length); 
    if (large_buffer == NULL) {
        perror("Memory allocation failed");
        return NULL;
    }

    // 循环接收数据，直到达到指定长度
    while (total_received < length) {
        size_t bytes_to_read = (length - total_received) < sizeof(small_buffer) ? (length - total_received) : sizeof(small_buffer);
        n = recv(client_sock, small_buffer, bytes_to_read, 0);
        if (n <= 0) {
            // 连接关闭或出错
            break;
        }
        // 将小缓冲区的数据复制到大缓存区
        memcpy(large_buffer + total_received, small_buffer, n);
        total_received += n;
    }

    if (total_received == length) {
        // 返回完整的数据
        return large_buffer;
    }

    // 释放大缓存区的内存
    free(large_buffer);

    return NULL;
}

// 将RequestPDU结构体和payload转换为一个buffer
char* requestToBuffer(RequestPDU *request, char *payload) {
    if (request == NULL) {
        return NULL;
    }

    // 计算请求头的长度
    size_t headerLength = 0;
    // 计算请求行的长度
    headerLength += strlen(request->method) + strlen(request->url) + strlen(request->version) + 4; // 4 是 " " 和 "\r\n" 的长度

    // 计算所有头字段的长度
    HeaderField *header = request->headers;
    while (header != NULL) {
        headerLength += strlen(header->key) + strlen(header->value) + 4; // 4 是 ": " 和 "\r\n" 的长度
        header = header->next;
    }
    headerLength += 2; // 空行的长度 "\r\n"

    // 计算payload的长度
    size_t payloadLength = payload ? strlen(payload) : 0;

    // 分配足够的内存
    char *buffer = (char *)malloc(headerLength + payloadLength + 1);
    if (buffer == NULL) {
        return NULL;
    }

    // 写入请求行
    size_t offset = 0;
    offset += snprintf(buffer + offset, headerLength + payloadLength + 1 - offset, "%s %s %s\r\n", request->method, request->url, request->version);

    // 写入头字段
    header = request->headers;
    while (header != NULL) {
        offset += snprintf(buffer + offset, headerLength + payloadLength + 1 - offset, "%s: %s\r\n", header->key, header->value);
        header = header->next;
    }
    // 写入空行
    offset += snprintf(buffer + offset, headerLength + payloadLength + 1 - offset, "\r\n");

    // 写入payload
    if (payload) {
        strcpy(buffer + offset, payload);
    }

    return buffer;
}

// 将ResponsePDU结构体和payload转换为一个buffer
char* responseToBuffer(ResponsePDU *response, char *payload) {
    if (response == NULL) {
        return NULL;
    }

    // 计算响应头的长度
    size_t headerLength = 0;
    // 计算状态行的长度
    headerLength += strlen(response->version) + strlen(response->status_code) + strlen(response->status_message) + 4; // 4 是 " " 和 "\r\n" 的长度

    // 计算所有头字段的长度
    HeaderField *header = response->headers;
    while (header != NULL) {
        headerLength += strlen(header->key) + strlen(header->value) + 4; // 4 是 ": " 和 "\r\n" 的长度
        header = header->next;
    }
    headerLength += 2; // 空行的长度 "\r\n"

    // 计算payload的长度
    size_t payloadLength = payload ? strlen(payload) : 0;

    // 分配足够的内存
    char *buffer = (char *)malloc(headerLength + payloadLength + 1);
    if (buffer == NULL) {
        return NULL;
    }

    // 写入状态行
    size_t offset = 0;
    offset += snprintf(buffer + offset, headerLength + payloadLength + 1 - offset, "%s %s %s\r\n", response->version, response->status_code, response->status_message);

    // 写入头字段
    header = response->headers;
    while (header != NULL) {
        offset += snprintf(buffer + offset, headerLength + payloadLength + 1 - offset, "%s: %s\r\n", header->key, header->value);
        header = header->next;
    }
    // 写入空行
    offset += snprintf(buffer + offset, headerLength + payloadLength + 1 - offset, "\r\n");

    // 写入payload
    if (payload) {
        strcpy(buffer + offset, payload);
    }

    return buffer;
}
// handle_response_header 处理响应头字段 添加server字段用于标识代理，将connection改回keep-alive
int handle_response_header(ResponsePDU *response,int flag){
    // 添加server字段
    HeaderField *server = createHeaderField("Server","rps/1.0.0");
    if (server==NULL){
        return SERVER_MALLOC_ERROR;
    }
    server->next=response->headers;
    response->headers=server;
    // 判断先前是不是keep-alive，如果是的话就修改否则不修改;
    if (flag){
        HeaderField *connection = search_header_field(response->headers,"Connection");
        char temp[20];
        sprintf(temp,"%s","keep-alive");
        if (connection!=NULL){
            free(connection->value);
            connection->value=strdup(temp);
        }else{
            connection=createHeaderField("Connection",temp);
            connection->next=response->headers;
            response->headers=connection;
        }
    }
    return 0;
}