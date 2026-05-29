#pragma once

#include <openssl/crypto.h>

#include <string>
class ssl_context
{
   public:
    ssl_context();
    ~ssl_context();

    bool init(const std::string& _CA);

    SSL* create_SSL(int fd);

    SSL_CTX* ctx() const;

   private:
    SSL_CTX* ctx_;
};