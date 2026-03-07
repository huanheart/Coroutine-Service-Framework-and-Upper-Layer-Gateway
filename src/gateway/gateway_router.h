#ifndef GATEWAY_ROUTER_H
#define GATEWAY_ROUTER_H

#include <string>
#include "../util/address.h"

class GatewayRouter {
public:
    static sylar::Address::ptr pick_upstream(const std::string& path);
};

#endif

