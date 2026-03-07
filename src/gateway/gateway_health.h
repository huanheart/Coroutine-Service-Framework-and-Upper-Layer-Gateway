#ifndef GATEWAY_HEALTH_H
#define GATEWAY_HEALTH_H

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include "../CoroutineLibrary/ioscheduler.h"
#include "../util/address.h"
#include "../util/socket.h"
#include "gateway_config.h"

class GatewayHealthManager {
public:
    static GatewayHealthManager& instance();

    void start();
    void stop();
    bool is_healthy(const std::string& key) const;
    uint64_t last_activity_ms(const std::string& key) const;
    bool should_disconnect_idle(const std::string& key) const;

private:
    GatewayHealthManager() = default;
    ~GatewayHealthManager() = default;

    void tick();
    static std::string key_from_addr(const sylar::Address::ptr& addr);
    static std::string key_from_target(const std::string& target);

private:
    struct Node {
        bool healthy = true;
        int fail_count = 0;
        uint64_t last_heartbeat_ms = 0;
        uint64_t last_activity_ms = 0;
    };
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, Node> m_nodes;
    std::shared_ptr<sylar::Timer> m_timer;
    std::unordered_map<std::string, sylar::Socket::ptr> m_hb_sockets;
};

#endif
