#ifndef GATEWAY_FILTER_CHAIN_H
#define GATEWAY_FILTER_CHAIN_H

#include <memory>
#include <string>
#include <vector>

#include "../util/socket.h"

struct GatewayRequestContext {
    std::string method;
    std::string path;
    std::string version;
    std::string client_address;
    std::string raw_request;
    sylar::Socket::ptr client;
};

class GatewayFilter {
public:
    virtual ~GatewayFilter() {}
    virtual bool apply(GatewayRequestContext& ctx, int& status, std::string& body) = 0;
};

class GatewayFilterChain {
public:
    static GatewayFilterChain& instance();

    bool apply(GatewayRequestContext& ctx, int& status, std::string& body);

private:
    GatewayFilterChain();

    void init_default();

private:
    std::vector<std::unique_ptr<GatewayFilter>> m_filters;
    bool m_inited;
};

#endif

