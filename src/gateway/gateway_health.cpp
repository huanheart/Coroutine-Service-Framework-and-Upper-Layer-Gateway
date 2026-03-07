#include "gateway_health.h"
#include "gateway_config.h"
#include <chrono>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <muduo/base/Logging.h>
#include "../util/config.h"

GatewayHealthManager& GatewayHealthManager::instance() {
    static GatewayHealthManager inst;
    return inst;
}

void GatewayHealthManager::start() {
    auto scheduler = sylar::IOManager::GetThis();
    if (!scheduler) {
        return;
    }
    int interval = GatewayConfig::instance().heartbeat_interval_ms();
    if (interval <= 0 || !GatewayConfig::instance().heartbeat_enabled()) {
        return;
    }
    m_timer = scheduler->addTimer(interval, [this]() { tick(); }, true);
    if (Config::get_instance()->get_close_log() == 0) {
        LOG_INFO << "health manager started interval_ms=" << interval;
    }
}

void GatewayHealthManager::stop() {
    std::lock_guard<std::mutex> lg(m_mutex);
    if (m_timer) {
        m_timer->cancel();
        m_timer.reset();
    }
}

bool GatewayHealthManager::is_healthy(const std::string& key) const {
    std::lock_guard<std::mutex> lg(m_mutex);
    auto it = m_nodes.find(key);
    if (it == m_nodes.end()) return true;
    return it->second.healthy;
}

uint64_t GatewayHealthManager::last_activity_ms(const std::string& key) const {
    std::lock_guard<std::mutex> lg(m_mutex);
    auto it = m_nodes.find(key);
    if (it == m_nodes.end()) return 0;
    return it->second.last_activity_ms;
}

bool GatewayHealthManager::should_disconnect_idle(const std::string& key) const {
    std::lock_guard<std::mutex> lg(m_mutex);
    auto it = m_nodes.find(key);
    if (it == m_nodes.end()) return false;
    uint64_t last = it->second.last_activity_ms;
    if (last == 0) return false;
    uint64_t now = (uint64_t) (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    int idle_ms = GatewayConfig::instance().idle_disconnect_ms();
    return (idle_ms > 0) && (now > last) && (now - last >= (uint64_t)idle_ms);
}

std::string GatewayHealthManager::key_from_addr(const sylar::Address::ptr& addr) {
    return addr ? addr->toString() : std::string();
}
std::string GatewayHealthManager::key_from_target(const std::string& target) {
    auto ip = sylar::Address::LookupAnyIPAddress(target);
    return key_from_addr(ip);
}

void GatewayHealthManager::tick() {
    GatewayConfig& cfg = GatewayConfig::instance();
    std::string path = cfg.heartbeat_path();
    int cto = cfg.timeout_connect_ms();
    int sto = cfg.timeout_send_ms();
    int rto = cfg.timeout_recv_ms();
    auto now = (uint64_t) (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    std::vector<std::string> targets;
    for (const auto& r : cfg.routes()) {
        for (const auto& t : r.targets) {
            std::string key = key_from_target(t);
            if (!key.empty()) {
                targets.push_back(key);
            }
        }
        for (const auto& ip : r.resolved_targets) {
            std::string key = key_from_addr(ip);
            if (!key.empty()) {
                targets.push_back(key);
            }
        }
    }
    // de-dup
    std::sort(targets.begin(), targets.end());
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());

    for (const auto& key : targets) {
        sylar::Address::ptr addr = sylar::Address::LookupAnyIPAddress(key);
        if (!addr) {
            std::lock_guard<std::mutex> lg(m_mutex);
            auto& n = m_nodes[key];
            n.healthy = false;
            n.fail_count++;
            n.last_heartbeat_ms = now;
            continue;
        }
        sylar::Socket::ptr sock;
        {
            std::lock_guard<std::mutex> lg(m_mutex);
            auto it = m_hb_sockets.find(key);
            if (it != m_hb_sockets.end()) {
                sock = it->second;
            }
        }
        bool need_connect = !sock || !sock->isConnected();
        if (need_connect) {
            sock = sylar::Socket::CreateTCP(addr);
            if (!sock || !sock->connect(addr, cto)) {
                std::lock_guard<std::mutex> lg(m_mutex);
                auto& n = m_nodes[key];
                n.healthy = false;
                n.fail_count++;
                n.last_heartbeat_ms = now;
                if (Config::get_instance()->get_close_log() == 0) {
                    LOG_INFO << "health hb connect failed key=" << key;
                }
                continue;
            }
            int on = 1;
            sock->setOption(SOL_SOCKET, SO_KEEPALIVE, on);
            sock->setSendTimeout(sto);
            sock->setRecvTimeout(rto);
            {
                std::lock_guard<std::mutex> lg(m_mutex);
                m_hb_sockets[key] = sock;
            }
        } else {
            sock->setSendTimeout(sto);
            sock->setRecvTimeout(rto);
        }
        std::string hb = "HEAD " + (path.empty() ? std::string("/") : path) + " HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
        const char* data = hb.data();
        size_t left = hb.size();
        bool ok = true;
        while (left > 0) {
            int n = sock->send(data, left, 0);
            if (n <= 0) { ok = false; break; }
            data += n;
            left -= n;
        }
        std::string resp;
        int rn = -1;
        if (ok) {
            char buf[512];
            for (int i = 0; i < 4; ++i) {
                int n = sock->recv(buf, sizeof(buf), 0);
                if (n <= 0) { rn = n; break; }
                resp.append(buf, n);
                if (resp.find("\r\n\r\n") != std::string::npos) { rn = (int)resp.size(); break; }
            }
        }
        uint64_t last_activity_ms = 0;
        if (rn > 0) {
            std::string keyhdr = "X-Last-Activity:";
            size_t pos = resp.find(keyhdr);
            if (pos != std::string::npos) {
                size_t eol = resp.find("\r\n", pos);
                std::string line = resp.substr(pos + keyhdr.size(), eol == std::string::npos ? std::string::npos : (eol - pos - keyhdr.size()));
                size_t s = line.find_first_not_of(" \t");
                size_t e = line.find_last_not_of(" \t");
                if (s != std::string::npos) {
                    std::string num = line.substr(s, e == std::string::npos ? std::string::npos : (e - s + 1));
                    last_activity_ms = std::strtoull(num.c_str(), nullptr, 10);
                }
            }
        }
        {
            std::lock_guard<std::mutex> lg(m_mutex);
            auto& n = m_nodes[key];
            n.last_heartbeat_ms = now;
            if (rn > 0) {
                n.healthy = true;
                n.fail_count = 0;
                if (last_activity_ms > 0) {
                    n.last_activity_ms = last_activity_ms;
                }
            } else {
                n.fail_count++;
                if (n.fail_count >= 3) {
                    n.healthy = false;
                }
            }
        }
        if (Config::get_instance()->get_close_log() == 0) {
            LOG_INFO << "health hb key=" << key << " ok=" << (rn > 0) << " last_activity_ms=" << last_activity_ms;
        }
    }
}
