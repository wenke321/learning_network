#pragma once

#include <cstring>
#include <forward_list>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "../../third-party/JSON/include/json.hpp"
#include "../../third-party/llhttp/include/llhttp.h"
#include "../TcpClient.h"
#include "../helpers/builtins.h"
#include "ssl_TcpConnection.h"
#include "ssl_context.h"

struct http_response
{
    int status_code;
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    nlohmann::json body_json() const { return nlohmann::json::parse(body); }
    void clear() { memset(_addressof(status_code), 0, sizeof(http_response)); }
};

class http_client
{
   public:
    typedef std::function<void(const http_response)> response_callback;
    typedef std::function<void(const std::string&)> error_callback;
    // typedef std::shared_ptr<ssl_TcpConnection> ssl_tcpconnection_ptr;

    http_client(EventLoop* _loop, const std::string& _CA);
    ~http_client();

    void get(const std::string& url, const std::unordered_map<std::string, std::string>& headers, response_callback resp_cb, error_callback errCb, bool keep_alive);
    void post(const std::string& url, const std::unordered_map<std::string, std::string>& headers, std::string body, response_callback cb, error_callback errCb, bool keep_alive);

    void force_close(const std::string _full_url);

    void set_close_callback(std::function<void()> _cb);

   private:
    void on_connection(const TcpConnectionPtr&);
    void on_message(const TcpConnectionPtr&, Buffer*);

    bool parse_url_https(const std::string& url, std::string& host, uint16_t& port, std::string& path);
    void resetParser();
    void handleError(const std::string& msg);

    static int on_status(llhttp_t* _parser, const char* _at, size_t _length);
    static int on_header_field(llhttp_t* _parser, const char* _at, size_t _length);
    static int on_hader_val(llhttp_t* _parser, const char* at, size_t length);
    static int on_body(llhttp_t* parser, const char* at, size_t length);
    static int on_header_complete(llhttp_t* parser);
    static int on_msg_complete(llhttp_t* parser);

    llhttp_t parser;
    llhttp_settings_t parser_settings;
    http_response response;
    bool headers_complete;
    bool msg_complete;
    std::function<void()> close_cb;
    std::string cur_header_field;
    std::string cur_header_val;
    response_callback resp_cb;
    error_callback err_cb;
    std::unordered_map<std::string, TcpClient*> clients;
    std::shared_ptr<ssl_context> ssl_ctx;
    std::forward_list<Buffer> request_bufs;
    Buffer response_buf;
    EventLoop* loop_;
    std::string version;
};