#include "ssl_context.h"

#include <openssl/prov_ssl.h>
#include <openssl/ssl.h>

#include "../Loggers/Logger.h"

ssl_context::ssl_context()
{
    ctx_ = SSL_CTX_new(TLS_client_method());
    if (!ctx_)
    {
        LOG_ERROR << " ";
    }
    SSL_CTX_set_min_proto_version(ctx_, TLS1_2_VERSION);
}
ssl_context::~ssl_context()
{
    if (ctx_) SSL_CTX_free(ctx_);
}

bool ssl_context::init(const std::string& _CA)
{
    {
        LOG_DEBUG << " ";
    }
    if (!SSL_CTX_load_verify_locations(ctx_, _CA.data(), nullptr))
    {
        {
            LOG_DEBUG << " invalid CA file";
        }
        return false;
    }
    SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER, NULL);
    return true;
}

SSL* ssl_context::create_SSL(int fd)
{
    {
        LOG_DEBUG << " ";
    }
    SSL* ssl = SSL_new(ctx_);
    SSL_set_fd(ssl, fd);
    return ssl;
}

SSL_CTX* ssl_context::ctx() const { return ctx_; }