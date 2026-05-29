#include "http_util.h"

#include <cstring>
#include <string>

std::string http_util::splice_request(const char* _method, const char* _source_url, const char* _version, std::string& _headers, std::string& _bodys)
{
    std::string buf(128, 0);
    buf.append(_method);
    buf.append(" ");
    buf.append(_source_url);
    buf.append(" ");
    buf.append("http/");
    buf.append(_version);
    buf.append("\r\n");
    buf.append(_headers.data(), _headers.size());
    buf.append(_bodys.data(), _bodys.size());
    return buf;
}

bool http_util::parse_url_https(const std::string& url, std::string& host, uint16_t& port, std::string& path)
{
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