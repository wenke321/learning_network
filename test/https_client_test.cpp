#include "../src/https/https_client.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "EventLoop.h"
#include "Loggers/Logger.h"

int main()
{
    {
        LOG_DEBUG << " ";
    }
    EventLoop loop;
    std::unique_ptr<http_client> client = std::make_unique<http_client>(&loop, "/etc/ssl/certs/ca-certificates.crt");

    nlohmann::json req_json;

    req_json["model"]    = "deepseek-v4-flash";
    req_json["messages"] = {{{"role", "user"}, {"content", "Hi"}}, {{"role", "system"}, {"content", "You are a helpful assistant"}}};
    // req_json["thinking"]         = {"type", "enabled"};
    req_json["reasoning_effort"] = "low";

    std::unordered_map<std::string, std::string> headers;
    headers["Authorization"] = "Bearer sk-9626f757f81742d08e482ae6df0deeed";
    headers["Content-Type"]  = "application/json";
    headers["Accept"]        = "application/json";

    // client.get(
    //     "https://api.deepseek.com/user/balance", headers,
    //     [&](const http_response& resp)
    //     {
    //         printf("Status: %d\n", resp.status_code);
    //         printf("Body: %s\n", resp.body.c_str());
    //         try
    //         {
    //             // auto json = resp.json();
    //             // printf("Login: %s\n", json["login"].get<std::string>().c_str());
    //             LOG_DEBUG << " status: " << resp.status_code << "\n";
    //             for (auto it : resp.headers)
    //             {
    //                 LOG_DEBUG << " " << it.first << ": " << it.second << "\n";
    //             }
    //             LOG_DEBUG << " body: " << resp.body;
    //         }
    //         catch (const std::exception& e)
    //         {
    //             LOG_ERROR << "JSON parse error: %s\n" << e.what();
    //         }
    //         // loop.quit_();
    //     },
    //     [&](const std::string& err)
    //     {
    //         fprintf(stderr, "Request error: %s\n", err.c_str());
    //         loop.quit_();
    //     },
    //     false);

    client->post(
        "https://api.deepseek.com/chat/completions", headers, req_json.dump(),
        [&](const http_response& resp)
        {
            printf("Status: %d\n", resp.status_code);
            printf("Body: %s\n", resp.body.c_str());
            // loop.quit_();
        },
        [&](const std::string& err)
        {
            fprintf(stderr, "Request error: %s\n", err.c_str());
            loop.quit_();
        },
        true);
    client->post(
        "https://api.deepseek.com/chat/completions", headers, req_json.dump(),
        [&](const http_response& resp)
        {
            printf("Status: %d\n", resp.status_code);
            printf("Body: %s\n", resp.body.c_str());
            // loop.quit_();
        },
        [&](const std::string& err)
        {
            fprintf(stderr, "Request error: %s\n", err.c_str());
            loop.quit_();
        },
        true);

    loop.Loop();
    return 0;
}