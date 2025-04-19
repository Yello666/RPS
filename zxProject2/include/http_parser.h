#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

/*
    RequestPDU 设计
*/
// 定义单个HTTP头字段键值对
typedef struct HeaderField {
    char *key;                  // 头字段名（如"Host"）
    char *value;                // 头字段值（如"example.com"）
    struct HeaderField *next;   // 指向下一个头字段的指针
}HeaderField;
// 定义HTTP请求头PDU
typedef struct {
    char *method;               // 请求方法（GET/POST等）
    char *url;                  // 请求URI（如"/index.html"）
    char *version;              // 协议版本（如"HTTP/1.1"）
    HeaderField *headers;       // 头字段链表
    // Todo request body
}RequestPDU;

// 定义HTTP响应头PDU
typedef struct {
    char *version;              // 协议版本（如"HTTP/1.1"）
    char *status_code;          // 状态码（如"204"）
    char *status_message;       // 状态消息（如"No Content"）
    HeaderField *headers;       // 头字段链表
    // Todo response body
} ResponsePDU;


// 解析HTTP请求
RequestPDU *parseHttpRequest(const char *data);

// 处理HTTP请求头
int handle_request_header(RequestPDU *request);

// 释放请求PDU的内存
void freeRequestPDU(RequestPDU *request);

// 解析HTTP响应
ResponsePDU *parseHttpResponse(const char *data);

// 处理HTTP响应头
int handle_response_header(ResponsePDU *response,int flag);

// 释放responsePDU内存
void freeResponsePDU(ResponsePDU *response);

char* read_payload(int client_sock,int length);

// 搜索特定字段
HeaderField *search_header_field(HeaderField *current,char *key);

// 解析请求，合并payload到buffer
char* requestToBuffer(RequestPDU *request, char *payload);

// 解析响应，合并payload到buffer
char* responseToBuffer(ResponsePDU *response, char *payload);

#endif