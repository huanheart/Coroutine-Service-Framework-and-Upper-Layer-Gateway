#include "gateway_router.h"
#include "gateway_config.h"

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
    if (!rc->resolved_targets.empty()) {
        return rc->resolved_targets[i % rc->resolved_targets.size()];
    }
    const string& target = rc->targets[i % rc->targets.size()];
    return sylar::Address::LookupAnyIPAddress(target);
}
