#include "gateway_config.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include "../util/gateway_constants.h"
#include "../util/jwt_cache.h"

GatewayConfig& GatewayConfig::instance() {
    static GatewayConfig cfg;
    // if (!cfg.m_inited) {
    //     throw std::runtime_error("GatewayConfig not inited");
    // }
    return cfg;
}

GatewayConfig::GatewayConfig()
    : m_max_qps(0),
      m_inited(false) {
}

const std::vector<RouteConfig>& GatewayConfig::routes() const {
    return m_routes;
}

const std::vector<std::string>& GatewayConfig::blacklist() const {
    return m_blacklist;
}

int GatewayConfig::max_qps() const {
    return m_max_qps;
}

const std::string& GatewayConfig::required_token() const {
    return m_required_token;
}

int64_t GatewayConfig::max_body_size() const {
    return m_max_body_size;
}

int GatewayConfig::timeout_connect_ms() const {
    return m_timeout_connect;
}

int GatewayConfig::timeout_send_ms() const {
    return m_timeout_send;
}

int GatewayConfig::timeout_recv_ms() const {
    return m_timeout_recv;
}

int GatewayConfig::heartbeat_interval_ms() const {
    return m_heartbeat_interval;
}

const std::string& GatewayConfig::heartbeat_path() const {
    return m_heartbeat_path;
}

bool GatewayConfig::heartbeat_enabled() const {
    return m_heartbeat_enabled;
}

int GatewayConfig::idle_disconnect_ms() const {
    return m_idle_disconnect_ms;
}

void GatewayConfig::set_config_path(const std::string& p) {
    std::lock_guard<std::mutex> lg(m_mutex);
    m_config_path = p;
}

const std::string& GatewayConfig::config_path() const {
    return m_config_path;
}

bool GatewayConfig::jwt_enabled() const {
    return m_jwt_enabled;
}

const std::string& GatewayConfig::jwt_secret() const {
    return m_jwt_secret;
}

const std::string& GatewayConfig::jwt_issuer() const {
    return m_jwt_issuer;
}

const std::string& GatewayConfig::jwt_audience() const {
    return m_jwt_audience;
}

const std::string& GatewayConfig::jwt_alg() const {
    return m_jwt_alg;
}

const RouteConfig* GatewayConfig::match_route(const std::string& path) const {
    std::lock_guard<std::mutex> lg(m_mutex);
    auto it = m_exact_map.find(path);
    if (it != m_exact_map.end()) {
        return it->second;
    }
    const TrieNode* node = m_trie_root.get();
    const RouteConfig* best = nullptr;
    if (!node) {
        return nullptr;
    }
    for (unsigned char c : path) {
        if (!node->children[c]) {
            break;
        }
        node = node->children[c].get();
        if (node->route) {
            best = node->route;
        }
    }
    return best;
}

bool GatewayConfig::load_from_file(const std::string& path) {
    std::ifstream in(path.c_str());
    if (!in.is_open()) {
        return false;
    }

    std::vector<RouteConfig> routes;
    std::vector<std::string> blacklist;
    int max_qps = 0;
    std::unordered_map<std::string, std::string> route_tokens;
    int64_t max_body_size = m_max_body_size;
    int timeout_connect = m_timeout_connect;
    int timeout_send = m_timeout_send;
    int timeout_recv = m_timeout_recv;
    int heartbeat_interval = m_heartbeat_interval;
    std::string heartbeat_path = m_heartbeat_path;
    bool heartbeat_enabled = m_heartbeat_enabled;
    int idle_disconnect_ms = m_idle_disconnect_ms;
    bool jwt_enabled = m_jwt_enabled;
    std::string jwt_secret = m_jwt_secret;
    std::string jwt_issuer = m_jwt_issuer;
    std::string jwt_audience = m_jwt_audience;
    std::string jwt_alg = m_jwt_alg;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream ss(line);
        std::string type;
        ss >> type;
        if (type == GatewayConst::CFG_ROUTE) {
            std::string prefix;
            std::string targets;
            ss >> prefix >> targets;
            if (prefix.empty() || targets.empty()) {
                continue;
            }
            RouteConfig rc;
            rc.path_prefix = prefix;
            rc.exact = false;
            std::stringstream ts(targets);
            std::string item;
            while (std::getline(ts, item, ',')) {
                if (!item.empty()) {
                    rc.targets.push_back(item);
                }
            }
            if (!rc.targets.empty()) {
                routes.push_back(rc);
            }
        } else if (type == GatewayConst::CFG_EXACT) {
            std::string ep;
            std::string targets;
            ss >> ep >> targets;
            if (ep.empty() || targets.empty()) {
                continue;
            }
            RouteConfig rc;
            rc.path_prefix = ep;
            rc.exact = true;
            std::stringstream ts(targets);
            std::string item;
            while (std::getline(ts, item, ',')) {
                if (!item.empty()) {
                    rc.targets.push_back(item);
                }
            }
            if (!rc.targets.empty()) {
                routes.push_back(rc);
            }
        } else if (type == GatewayConst::CFG_BLACK) {
            std::string ip;
            ss >> ip;
            if (!ip.empty()) {
                blacklist.push_back(ip);
            }
        } else if (type == GatewayConst::CFG_QPS) {
            int qps = 0;
            ss >> qps;
            if (qps > 0) {
                max_qps = qps;
            }
        } else if (type == GatewayConst::CFG_TOKEN) {
            std::string pfx;
            std::string token;
            ss >> pfx >> token;
            if (!pfx.empty() && !token.empty()) {
                route_tokens[pfx] = token;
            }
        } else if (type == GatewayConst::CFG_MAX_BODY_SIZE) {
            int64_t v = 0;
            ss >> v;
            if (v > 0) {
                max_body_size = v;
            }
        } else if (type == GatewayConst::CFG_TIMEOUT_CONNECT_MS) {
            int v = 0;
            ss >> v;
            if (v > 0) {
                timeout_connect = v;
            }
        } else if (type == GatewayConst::CFG_TIMEOUT_SEND_MS) {
            int v = 0;
            ss >> v;
            if (v > 0) {
                timeout_send = v;
            }
        } else if (type == GatewayConst::CFG_TIMEOUT_RECV_MS) {
            int v = 0;
            ss >> v;
            if (v > 0) {
                timeout_recv = v;
            }
        } else if (type == GatewayConst::CFG_HEARTBEAT_INTERVAL_MS) {
            int v = 0;
            ss >> v;
            if (v > 0) {
                heartbeat_interval = v;
            }
        } else if (type == GatewayConst::CFG_HEARTBEAT_PATH) {
            std::string v;
            ss >> v;
            heartbeat_path = v;
        } else if (type == GatewayConst::CFG_ENABLE_HEARTBEAT) {
            std::string v;
            ss >> v;
            heartbeat_enabled = (v == "on" || v == "true" || v == "1");
        } else if (type == GatewayConst::CFG_IDLE_DISCONNECT_MS) {
            int v = 0;
            ss >> v;
            if (v > 0) {
                idle_disconnect_ms = v;
            }
        } else if (type == GatewayConst::CFG_JWT_ENABLED) {
            std::string v;
            ss >> v;
            jwt_enabled = (v == "on" || v == "true" || v == "1");
        } else if (type == GatewayConst::CFG_JWT_SECRET) {
            std::string v;
            ss >> v;
            jwt_secret = v;
        } else if (type == GatewayConst::CFG_JWT_ISSUER) {
            std::string v;
            ss >> v;
            jwt_issuer = v;
        } else if (type == GatewayConst::CFG_JWT_AUDIENCE) {
            std::string v;
            ss >> v;
            jwt_audience = v;
        } else if (type == GatewayConst::CFG_JWT_ALG) {
            std::string v;
            ss >> v;
            jwt_alg = v;
        } else if (type == GatewayConst::CFG_JWT_CACHE_CAPACITY) {
            int v = 0;
            ss >> v;
            if (v > 0) {
                JwtCache::instance().set_capacity(v);
            }
        }
    }

    if (routes.empty()) {
        return false;
    }
    bool has_default = false;
    for (auto& r : routes) {
        if (!r.exact && r.path_prefix == "/") {
            has_default = true;
            break;
        }
    }
    if (!has_default) {
        return false;
    }

    for (auto& r : routes) {
        r.resolved_targets.clear();
        for (auto& t : r.targets) {
            auto ip = sylar::Address::LookupAnyIPAddress(t);
            if (ip) {
                r.resolved_targets.push_back(ip);
            }
        }
        auto it = route_tokens.find(r.path_prefix);
        if (it != route_tokens.end()) {
            r.required_token = it->second;
        }
    }

    {
        std::lock_guard<std::mutex> lg(m_mutex);
        m_routes = routes;
        m_blacklist = blacklist;
        if (max_qps > 0) {
            m_max_qps = max_qps;
        }
        m_max_body_size = max_body_size;
        m_timeout_connect = timeout_connect;
        m_timeout_send = timeout_send;
        m_timeout_recv = timeout_recv;
        m_heartbeat_interval = heartbeat_interval;
        m_heartbeat_path = heartbeat_path;
        m_heartbeat_enabled = heartbeat_enabled;
        m_idle_disconnect_ms = idle_disconnect_ms;
        m_jwt_enabled = jwt_enabled;
        m_jwt_secret = jwt_secret;
        m_jwt_issuer = jwt_issuer;
        m_jwt_audience = jwt_audience;
        m_jwt_alg = jwt_alg;
        m_config_path = path;
        build_trie();
        JwtCache::instance().clear();
        m_inited = true;
    }
    return true;
}

bool GatewayConfig::reload() {
    if (m_config_path.empty()) {
        return false;
    }
    return load_from_file(m_config_path);
}


void GatewayConfig::build_trie() {
    m_trie_root.reset(new TrieNode());
    m_exact_map.clear();
    for (const auto& r : m_routes) {
        if (r.exact) {
            m_exact_map[r.path_prefix] = &r;
            continue;
        }
        TrieNode* node = m_trie_root.get();
        for (unsigned char c : r.path_prefix) {
            if (!node->children[c]) {
                node->children[c] = std::unique_ptr<TrieNode>(new TrieNode());
            }
            node = node->children[c].get();
        }
        node->route = &r;
    }
}
