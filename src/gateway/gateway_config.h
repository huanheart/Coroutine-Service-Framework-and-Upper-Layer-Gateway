#ifndef GATEWAY_CONFIG_H
#define GATEWAY_CONFIG_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "../util/address.h"

struct RouteConfig {
    std::string path_prefix;
    //保留原始字符串地址(也方便人为观看)
    std::vector<std::string> targets;
    //减少每次请求的解析开销
    std::vector<sylar::IPAddress::ptr> resolved_targets;
    bool exact = false;
    std::string required_token;
};

class GatewayConfig {
public:
    static GatewayConfig& instance();

    const std::vector<RouteConfig>& routes() const;
    const std::vector<std::string>& blacklist() const;
    int max_qps() const;
    const std::string& required_token() const;
    int64_t max_body_size() const;
    int timeout_connect_ms() const;
    int timeout_send_ms() const;
    int timeout_recv_ms() const;
    int heartbeat_interval_ms() const;
    const std::string& heartbeat_path() const;
    bool heartbeat_enabled() const;
    int idle_disconnect_ms() const;
    void set_config_path(const std::string& p);
    const std::string& config_path() const;
    bool jwt_enabled() const;
    const std::string& jwt_secret() const;
    const std::string& jwt_issuer() const;
    const std::string& jwt_audience() const;
    const std::string& jwt_alg() const;

    const RouteConfig* match_route(const std::string& path) const;

    bool load_from_file(const std::string& path);
    bool reload();

private:
    GatewayConfig();

    void build_trie();

private:
    std::vector<RouteConfig> m_routes;
    std::vector<std::string> m_blacklist;
    int m_max_qps;
    std::string m_required_token;
    std::string m_config_path;
    bool m_inited;
    int64_t m_max_body_size = 4 * 1024 * 1024;
    int m_timeout_connect = 2000;
    int m_timeout_send = 2000;
    int m_timeout_recv = 2000;
    int m_heartbeat_interval = 30000;
    std::string m_heartbeat_path;
    bool m_heartbeat_enabled = true;
    int m_idle_disconnect_ms = 180000;
    bool m_jwt_enabled = false;
    std::string m_jwt_secret;
    std::string m_jwt_issuer;
    std::string m_jwt_audience;
    std::string m_jwt_alg = "HS256";
    struct TrieNode {
        std::unique_ptr<TrieNode> children[256];
        const RouteConfig* route = nullptr;
    };
    //字典树匹配
    std::unique_ptr<TrieNode> m_trie_root;
    //用于精准匹配
    std::unordered_map<std::string, const RouteConfig*> m_exact_map;
    mutable std::mutex m_mutex;
};

#endif
