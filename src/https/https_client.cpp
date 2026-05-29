#include "https_client.h"

#include <memory>
#include <string>
#include <unordered_map>

#include "../Loggers/Logger.h"
#include "../Sockets/SocketOps.h"
#include "ssl_TcpConnection.h"
#include "ssl_context.h"

int http_client::on_status(llhttp_t* _parser, const char* _at, size_t _length)
{
    {
        LOG_DEBUG << " ";
    }
    auto* self                 = static_cast<http_client*>(_parser->data);
    self->response.status_code = _parser->status_code;
    return 0;
}
int http_client::on_header_field(llhttp_t* _parser, const char* _at, size_t _length)
{
    {
        LOG_DEBUG << " ";
    }
    auto* self    = static_cast<http_client*>(_parser->data);
    std::string f = self->cur_header_field;
    std::string v = self->cur_header_val;
    if (!f.empty() && !v.empty())
    {
        self->response.headers[f] = v;
        f.clear();
        v.clear();
    }
    f.append(_at, _length);
    return 0;
}
int http_client::on_hader_val(llhttp_t* _parser, const char* _at, size_t _length)
{
    {
        LOG_DEBUG << " ";
    }
    auto* self     = static_cast<http_client*>(_parser->data);
    std::string& v = self->cur_header_val;
    v.append(_at, _length);
    return 0;
}
int http_client::on_body(llhttp_t* _parser, const char* _at, size_t _length)
{
    {
        LOG_DEBUG << " ";
    }
    auto* self        = static_cast<http_client*>(_parser->data);
    std::string& body = self->response.body;
    body.append(_at, _length);
    return 0;
}
int http_client::on_header_complete(llhttp_t* _parser)
{
    {
        LOG_DEBUG << " ";
    }
    auto* self    = static_cast<http_client*>(_parser->data);
    std::string f = self->cur_header_field;
    if (!f.empty())
    {
        self->response.headers[f] = self->cur_header_val;
    }
    self->headers_complete = true;
    return 0;
}
int http_client::on_msg_complete(llhttp_t* _parser)
{
    {
        LOG_DEBUG << " ";
    }
    auto* self         = static_cast<http_client*>(_parser->data);
    self->msg_complete = true;
    return 0;
}

http_client::http_client(EventLoop* _loop, const std::string& _CA, bool _keep_alive) : headers_complete(false), msg_complete(false), loop_(_loop), ssl_ctx(std::make_shared<ssl_context>())
{
    {
        LOG_DEBUG << " ";
    }
    if (!ssl_ctx->init(_CA))
    {
        LOG_ERROR << " load CA failed";
    }

    llhttp_settings_init(&parser_settings);
    parser_settings.on_status           = on_status;
    parser_settings.on_header_field     = on_header_field;
    parser_settings.on_header_value     = on_hader_val;
    parser_settings.on_headers_complete = on_header_complete;
    parser_settings.on_body             = on_body;
    parser_settings.on_message_complete = on_msg_complete;
    llhttp_init(&parser, HTTP_RESPONSE, &parser_settings);
    parser.data = this;
}

http_client::~http_client() {}

void http_client::get(const std::string& url, const std::unordered_map<std::string, std::string>& headers, response_callback cb, error_callback errCb)
{
    {
        LOG_DEBUG << " ";
    }
    // 解析 URL
    std::string host, path;
    uint16_t port = 443;
    if (!parse_url_https(url, host, port, path))
    {
        if (errCb) errCb("Invalid URL: " + url);
        return;
    }

    // 重置解析器状态
    resetParser();

    // 保存回调
    resp_cb = std::move(cb);
    err_cb  = std::move(errCb);

    // 构造 HTTP 请求行和头部
    Buffer buf;
    buf.append("GET ");
    buf.append(path);
    buf.append(" HTTP/1.1\r\n");
    buf.append("Host: ");
    buf.append(host);
    buf.append("\r\n");
    buf.append("Connection: ");
    if (keep_alive)
        buf.append("keep-alive\r\n");
    else
        buf.append("close\r\n");
    // std::ostringstream req;
    // req << "GET " << path << " HTTP/1.1\r\n";
    // req << "Host: " << host << "\r\n";
    // req << "Connection: close\r\n";  // 暂时不保持连接
    // req << "Accept: */*\r\n";
    //  用户自定义头
    for (const auto& h : headers)
    {
        buf.append(h.first);
        buf.append(": ");
        buf.append(h.second);
        buf.append("\r\n");
        // req << h.first << ": " << h.second << "\r\n";
    }
    // req << "\r\n";
    buf.append("\r\n");
    request_bufs.emplace_front(std::move(buf));

    // 每次请求都新建 TcpClient，保证状态干净且可并发（每个实例一个）
    client_ = std::make_unique<TcpClient>(loop_, InetAddr(sockOption::resolve_domain_ipv4(host.data()), port, 0), "https_client", keep_alive);
    client_->enbale_ssl(ssl_ctx, host);  // 设置 SSL 并指定 SNI 主机名
    client_->setConnectionCallback([&](const TcpConnectionPtr& conn) { on_connection(conn); });
    client_->setMessageCallback([&](const TcpConnectionPtr& _conn, Buffer* _buf) { on_message(_conn, _buf); });
    client_->connect();
}

void http_client::post(const std::string& url, const std::unordered_map<std::string, std::string>& headers, std::string body, response_callback cb, error_callback errCb)
{
    // 解析 URL
    std::string host, path;
    uint16_t port = 443;
    if (!parse_url_https(url, host, port, path))
    {
        if (errCb) errCb("Invalid URL: " + url);
        return;
    }

    // 重置解析器状态
    resetParser();

    // 保存回调
    resp_cb = std::move(cb);
    err_cb  = std::move(errCb);

    // 构造 HTTP 请求行和头部
    Buffer buf;

    buf.append("POST ");
    buf.append(path);
    buf.append(" HTTP/1.1\r\n");

    buf.append("Host: ");
    buf.append(host);
    buf.append("\r\n");

    buf.append("Connection: ");
    if (keep_alive)
        buf.append("keep-alive\r\n");
    else
        buf.append("close\r\n");

    buf.append("Content-Length: ");
    buf.append(std::to_string(body.size()));
    buf.append("\r\n");

    // std::ostringstream req;
    // req << "GET " << path << " HTTP/1.1\r\n";
    // req << "Host: " << host << "\r\n";
    // req << "Connection: close\r\n";  // 暂时不保持连接
    // req << "Accept: */*\r\n";
    //  用户自定义头
    for (const auto& h : headers)
    {
        buf.append(h.first);
        buf.append(": ");
        buf.append(h.second);
        buf.append("\r\n");
        // req << h.first << ": " << h.second << "\r\n";
    }
    // req << "\r\n";
    buf.append("\r\n");
    buf.append(body);
    request_bufs.emplace_front(std::move(buf));

    // 每次请求都新建 TcpClient，保证状态干净且可并发（每个实例一个）
    client_ = std::make_unique<TcpClient>(loop_, InetAddr(sockOption::resolve_domain_ipv4(host.data()), port, 0), "https_client", keep_alive);
    client_->enbale_ssl(ssl_ctx, host);  // 设置 SSL 并指定 SNI 主机名
    client_->setConnectionCallback([&](const TcpConnectionPtr& conn) { on_connection(conn); });
    client_->setMessageCallback([&](const TcpConnectionPtr& _conn, Buffer* _buf) { on_message(_conn, _buf); });
    client_->connect();
}

void http_client::on_connection(const TcpConnectionPtr& _conn)
{
    {
        LOG_DEBUG << " ";
    }
    if (_conn->connected())
    {
        for (auto& it : request_bufs)
        {
            _conn->send(&it);
        }
        request_bufs.clear();
    }
    else
    {
        LOG_DEBUG << " ";
    }
}

void http_client::on_message(const TcpConnectionPtr& _conn, Buffer* _buf)
{
    {
        LOG_DEBUG << " ";
    }
    llhttp_errno_t ret = llhttp_execute(&parser, _buf->readBegin(), _buf->readableBytes());
    _buf->retrieveAll();

    if (ret == HPE_OK && msg_complete)
    {
        resp_cb(response);
        _conn->shutdown();
    }
    else if (ret != HPE_OK && ret != HPE_PAUSED)
    {
        {
            LOG_ERROR << " llhttp parse error: " << llhttp_errno_name(ret) << ",code=" << ret;
        }
        _conn->shutdown();
    }
}

bool http_client::parse_url_https(const std::string& url, std::string& host, uint16_t& port, std::string& path)
{
    {
        LOG_DEBUG << " ";
    }
    // 简易 URL 解析，支持 https:// 前缀
    const char* start = url.c_str();
    if (strncmp(start, "https://", 8) == 0)
    {
        start += 8;
    }
    else
    {
        return false;
    }

    const char* hostStart = start;
    const char* slash     = strchr(hostStart, '/');
    std::string hostPort;
    if (slash)
    {
        hostPort = std::string(hostStart, slash);
        path     = slash;  // 保留 '/'
    }
    else
    {
        hostPort = hostStart;
        path     = "/";
    }

    // 分离 host 和 port
    size_t colon = hostPort.find(':');
    if (colon != std::string::npos)
    {
        host = hostPort.substr(0, colon);
        port = static_cast<uint16_t>(std::stoi(hostPort.substr(colon + 1)));
    }
    else
    {
        host = hostPort;
        port = 443;
    }
    return true;
}

void http_client::resetParser()
{
    {
        LOG_DEBUG << " ";
    }
    llhttp_init(&parser, HTTP_RESPONSE, &parser_settings);
    parser.data      = this;
    headers_complete = false;
    msg_complete     = false;
    memset(&response, 0, sizeof(response));
    cur_header_field.clear();
    cur_header_val.clear();
}
