#include "../src/https/http_util.h"

#include <cstdint>
#include <cstring>
#include <iostream>

int main()
{
    std::cout << http_util::METHODS[GET] << "\n" << http_util::METHODS[PUT] << "\n" << http_util::METHODS[POST] << "\n" << http_util::METHODS[DELETE] << "\n" << http_util::METHODS[OPTIONS] << "\n" << http_util::METHODS[PATCH] << "\n" << http_util::METHODS[HEAD] << "\n" << http_util::METHODS[TRACE] << "\n" << http_util::METHODS[CONNECT] << "\n";
    std::string host, path;
    uint16_t port;
    http_util::parse_url_https("https://abc.com/register.do", host, port, path);
    std::cout << host << port << path << "\n";
}