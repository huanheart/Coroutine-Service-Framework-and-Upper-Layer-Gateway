#include "gateway_router.h"
#include "gateway_config.h"
#include "gateway_health.h"

#include <atomic>

using namespace std;

namespace {
std::atomic<size_t> s_index(0);
}

sylar::Address::ptr GatewayRouter::pick_upstream(const string& path) {
    GatewayConfig& cfg = GatewayConfig::instance();
    const RouteConfig* rc = cfg.match_route(path);
    if (!rc) {
        return nullptr;
    }
    if (rc->resolved_targets.empty() && rc->targets.empty()) {
        return nullptr;
    }
    size_t i = s_index.fetch_add(1, std::memory_order_relaxed);
    // prefer healthy targets
    std::vector<sylar::Address::ptr> healthy_addrs;
    for (auto& a : rc->resolved_targets) {
        std::string key = a->toString();
        if (GatewayHealthManager::instance().is_healthy(key)) {
            healthy_addrs.push_back(a);
        }
    }
    if (!healthy_addrs.empty()) {
        return healthy_addrs[i % healthy_addrs.size()];
    }
    // fallback: resolve string targets and filter healthy
    std::vector<sylar::Address::ptr> resolved;
    for (auto& t : rc->targets) {
        auto addr = sylar::Address::LookupAnyIPAddress(t);
        if (addr) {
            std::string key = addr->toString();
            if (GatewayHealthManager::instance().is_healthy(key)) {
                resolved.push_back(addr);
            }
        }
    }
    if (!resolved.empty()) {
        return resolved[i % resolved.size()];
    }
    // if none healthy, fallback to original strategy
    if (!rc->resolved_targets.empty()) {
        return rc->resolved_targets[i % rc->resolved_targets.size()];
    }
    const string& target = rc->targets[i % rc->targets.size()];
    return sylar::Address::LookupAnyIPAddress(target);
}
