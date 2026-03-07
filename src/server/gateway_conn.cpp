#include "gateway_conn.h"

#include <string>

#include "../gateway/gateway_filter_chain.h"
#include "../gateway/gateway_metrics.h"
#include "../gateway/gateway_router.h"
#include "../gateway/gateway_config.h"
#include "../gateway/gateway_health.h"
#include "../util/gateway_constants.h"
#include <muduo/base/Logging.h>
#include "../util/config.h"

using namespace std;

void gateway_conn::init(sylar::Socket::ptr& client) {
    m_client = client;
    m_read_idx = 0;
    m_request.clear();
    m_content_length = 0;
    if (!m_upstreams.empty()) {
        for (auto& kv : m_upstreams) {
            auto& ent = kv.second;
            if (ent.sock && ent.sock->isConnected()) {
                ent.sock->close();
            }
        }
        m_upstreams.clear();
    }
    m_oversize = false;
    m_curr_upstream_key.clear();
}

static inline uint64_t now_ms() {
    return (uint64_t) (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

void gateway_conn::prune_idle_entry(const std::string& key, gateway_conn::UpstreamEntry& ent) {
    if (!ent.sock) return;
    if (!ent.sock->isConnected()) return;
    if (ent.busy) return;
    int idle_ms = GatewayConfig::instance().idle_disconnect_ms();
    uint64_t now = now_ms();
    if (idle_ms <= 0) return;
    uint64_t last_used = ent.last_used_ms;
    uint64_t last_activity = GatewayHealthManager::instance().last_activity_ms(key);
    bool local_idle = (last_used > 0) && (now > last_used) && (now - last_used >= (uint64_t)idle_ms);
    bool remote_idle = (last_activity > 0) && (now > last_activity) && (now - last_activity >= (uint64_t)idle_ms);
    if (local_idle || remote_idle) {
        if (Config::get_instance()->get_close_log() == 0) {
            LOG_INFO << "gateway idle disconnect upstream " << key
                     << " local_idle=" << local_idle
                     << " remote_idle=" << remote_idle
                     << " idle_ms=" << idle_ms
                     << " last_used_ms=" << last_used
                     << " last_activity_ms=" << last_activity;
        }
        ent.sock->close();
    }
}

bool gateway_conn::read_once() {
    if (!m_client || !m_client->isValid()) {
        return false;
    }
    // read until headers complete
    while (m_request.find("\r\n\r\n") == string::npos) {
        if (m_read_idx >= READ_BUFFER_SIZE) {
            return false;
        }
        char* base = m_read_buf + m_read_idx;
        int n = m_client->recv(base, READ_BUFFER_SIZE - m_read_idx, 0);
        if (n <= 0) {
            return false;
        }
        m_read_idx += n;
        m_request.append(m_read_buf + (m_read_idx - n), n);
        if (m_request.size() > 64 * 1024) {
            return false;
        }
    }
    if (Config::get_instance()->get_close_log() == 0) {
        LOG_INFO << "gateway headers received size=" << m_request.size();
    }
    // parse content-length if present
    m_content_length = 0;
    {
        size_t pos = m_request.find("\r\n\r\n");
        string headers = m_request.substr(0, pos);
        string key = "Content-Length:";
        size_t cl = headers.find(key);
        if (cl != string::npos) {
            size_t end = headers.find("\r\n", cl);
            string line = headers.substr(cl + key.size(), end == string::npos ? string::npos : (end - cl - key.size()));
            size_t s = line.find_first_not_of(" \t");
            size_t e = line.find_last_not_of(" \t");
            if (s != string::npos) {
                string num = line.substr(s, e == string::npos ? string::npos : (e - s + 1));
                m_content_length = std::strtol(num.c_str(), nullptr, 10);
            }
        }
    }
    if (Config::get_instance()->get_close_log() == 0) {
        LOG_INFO << "gateway content-length=" << m_content_length;
    }
    {
        int64_t max_body = GatewayConfig::instance().max_body_size();
        if (m_content_length > 0 && (int64_t)m_content_length > max_body) {
            m_oversize = true;
            if (Config::get_instance()->get_close_log() == 0) {
                LOG_INFO << "gateway body oversize len=" << m_content_length << " max=" << max_body;
            }
            return true;
        }
    }
    // read body if any
    if (m_content_length > 0) {
        size_t pos = m_request.find("\r\n\r\n");
        size_t have = m_request.size() - (pos + 4);
        while (have < (size_t)m_content_length) {
            if (m_read_idx >= READ_BUFFER_SIZE) {
                m_read_idx = 0;
            }
            int n = m_client->recv(m_read_buf, READ_BUFFER_SIZE, 0);
            if (n <= 0) {
                return false;
            }
            m_request.append(m_read_buf, n);
            have += n;
        }
    }
    return true;
}

bool gateway_conn::parse_request_line(const string& request, string& method, string& path, string& version) {
    size_t pos = request.find("\r\n");
    if (pos == string::npos) return false;
    string line = request.substr(0, pos);
    size_t p1 = line.find(' ');
    if (p1 == string::npos) return false;
    size_t p2 = line.find(' ', p1 + 1);
    if (p2 == string::npos) return false;
    method = line.substr(0, p1);
    path = line.substr(p1 + 1, p2 - p1 - 1);
    version = line.substr(p2 + 1);
    return true;
}

void gateway_conn::send_simple_response(int status, const string& status_text, const string& body) {
    string headers = "HTTP/1.1 " + to_string(status) + " " + status_text + "\r\n"
                     "Content-Type: text/plain\r\n"
                     "Connection: close\r\n"
                     "Content-Length: " + to_string(body.size()) + "\r\n"
                     "\r\n";
    string data = headers + body;
    const char* buf = data.data();
    size_t left = data.size();
    while (left > 0) {
        int n = m_client->send(buf, left, 0);
        if (n <= 0) {
            break;
        }
        buf += n;
        left -= n;
    }
}

sylar::Socket::ptr gateway_conn::get_upstream_socket(const std::string& path) {
    prune_idle_upstreams();
    sylar::Address::ptr addr = GatewayRouter::pick_upstream(path);
    if (!addr) {
        return nullptr;
    }
    std::string key = addr->toString();
    if (Config::get_instance()->get_close_log() == 0) {
        LOG_INFO << "route select upstream " << key << " for path " << path;
    }
    auto it = m_upstreams.find(key);
    if (it != m_upstreams.end()) {
        auto& ent = it->second;
        if (ent.sock && ent.sock->isConnected()) {
            m_curr_upstream_key = key;
            ent.last_used_ms = now_ms();
            return ent.sock;
        }
    }
    sylar::Socket::ptr sock = sylar::Socket::CreateTCP(addr);
    if (!sock) {
        return nullptr;
    }
    int cto = GatewayConfig::instance().timeout_connect_ms();
    if (!sock->connect(addr, cto)) {
        sock->close();
        return nullptr;
    }
    int sto = GatewayConfig::instance().timeout_send_ms();
    int rto = GatewayConfig::instance().timeout_recv_ms();
    sock->setSendTimeout(sto);
    sock->setRecvTimeout(rto);
    int on = 1;
    sock->setOption(SOL_SOCKET, SO_KEEPALIVE, on);
    UpstreamEntry ent;
    ent.sock = sock;
    ent.last_used_ms = now_ms();
    m_upstreams[key] = ent;
    m_curr_upstream_key = key;
    return sock;
}

void gateway_conn::prune_idle_upstreams() {
    for (auto it = m_upstreams.begin(); it != m_upstreams.end(); ) {
        prune_idle_entry(it->first, it->second);
        if (!it->second.sock || !it->second.sock->isConnected()) {
            it = m_upstreams.erase(it);
        } else {
            ++it;
        }
    }
}

bool gateway_conn::process() {
    string request = m_request;
    m_request.clear();
    m_read_idx = 0;

    GatewayMetrics::instance().on_request();

    string method, path, version;
    if (!parse_request_line(request, method, path, version)) {
        if (Config::get_instance()->get_close_log() == 0) {
            LOG_INFO << "gateway bad request line";
        }
        send_simple_response(GatewayConst::HttpStatus::BAD_REQUEST, GatewayConst::status_text(GatewayConst::HttpStatus::BAD_REQUEST), "invalid request line\n");
        return false;
    }
    if (Config::get_instance()->get_close_log() == 0) {
        LOG_INFO << "gateway request " << method << " " << path << " " << version;
    }
    prune_idle_upstreams();
    if (path.rfind(GatewayConst::ADMIN_METRICS_PATH, 0) == 0) {
        string body = GatewayMetrics::instance().render_plain();
        if (Config::get_instance()->get_close_log() == 0) {
            LOG_INFO << "gateway admin metrics";
        }
        send_simple_response(GatewayConst::HttpStatus::OK, GatewayConst::status_text(GatewayConst::HttpStatus::OK), body);
        return false;
    }

    if (path.rfind(GatewayConst::ADMIN_RELOAD_PATH, 0) == 0) {
        bool ok = GatewayConfig::instance().reload();
        if (ok) {
            if (Config::get_instance()->get_close_log() == 0) {
                LOG_INFO << "gateway admin reload ok";
            }
            send_simple_response(GatewayConst::HttpStatus::OK, GatewayConst::status_text(GatewayConst::HttpStatus::OK), "reload ok\n");
        } else {
            if (Config::get_instance()->get_close_log() == 0) {
                LOG_INFO << "gateway admin reload failed";
            }
            send_simple_response(GatewayConst::HttpStatus::INTERNAL_ERROR, GatewayConst::status_text(GatewayConst::HttpStatus::INTERNAL_ERROR), "reload failed\n");
        }
        return false;
    }

    if (m_oversize) {
        send_simple_response(GatewayConst::HttpStatus::PAYLOAD_TOO_LARGE, GatewayConst::status_text(GatewayConst::HttpStatus::PAYLOAD_TOO_LARGE), "body too large\n");
        if (Config::get_instance()->get_close_log() == 0) {
            LOG_INFO << "gateway recevie too large message";
        }
        return false;
    }

    GatewayRequestContext ctx;
    ctx.method = method;
    ctx.path = path;
    ctx.version = version;
    ctx.raw_request = request;
    ctx.client = m_client;
    sylar::Address::ptr remote = m_client->getRemoteAddress();
    if (remote) {
        ctx.client_address = remote->toString();
    }
    int filter_status = 0;
    string filter_body;
    if (!GatewayFilterChain::instance().apply(ctx, filter_status, filter_body)) {
        GatewayMetrics::instance().on_filter_reject();
        int status = filter_status == 0 ? GatewayConst::HttpStatus::FORBIDDEN : filter_status;
        string text = GatewayConst::status_text(status);
        if (filter_body.empty()) {
            filter_body = "forbidden\n";
        }
        if (Config::get_instance()->get_close_log() == 0) {
            LOG_INFO << "gateway filter reject status=" << status;
        }
        send_simple_response(status, text, filter_body);
        return false;
    }

    sylar::Socket::ptr upstream = get_upstream_socket(path);
    if (!upstream) {
        if (Config::get_instance()->get_close_log() == 0) {
            LOG_INFO << "gateway no upstream available";
        }
        send_simple_response(GatewayConst::HttpStatus::BAD_GATEWAY, GatewayConst::status_text(GatewayConst::HttpStatus::BAD_GATEWAY), "no upstream available\n");
        return false;
    }

    {
        auto it = m_upstreams.find(m_curr_upstream_key);
        if (it != m_upstreams.end()) {
            it->second.busy = true;
            it->second.last_used_ms = now_ms();
        }
        const char* buf = request.data();
        size_t left = request.size();
        size_t total = 0;
        while (left > 0) {
            int n = upstream->send(buf, left, 0);
            if (n <= 0) {
                GatewayMetrics::instance().on_upstream_error();
                send_simple_response(GatewayConst::HttpStatus::BAD_GATEWAY, GatewayConst::status_text(GatewayConst::HttpStatus::BAD_GATEWAY), "send to upstream failed\n");
                return false;
            }
            buf += n;
            left -= n;
            total += n;
        }
        if (Config::get_instance()->get_close_log() == 0) {
            LOG_INFO << "gateway forwarded request bytes=" << total;
        }
    }

    char buf[4096];
    size_t total_up = 0;
    size_t total_down = 0;
    while (true) {
        int n = upstream->recv(buf, sizeof(buf), 0);
        if (n <= 0) {
            break;
        }
        total_up += n;
        int sent = 0;
        while (sent < n) {
            int s = m_client->send(buf + sent, n - sent, 0);
            if (s <= 0) {
                return false;
            }
            sent += s;
            total_down += s;
        }
    }

    {
        auto it = m_upstreams.find(m_curr_upstream_key);
        if (it != m_upstreams.end()) {
            it->second.busy = false;
            it->second.last_used_ms = now_ms();
        }
    }
    if (Config::get_instance()->get_close_log() == 0) {
        LOG_INFO << "gateway upstream recv bytes=" << total_up << " client send bytes=" << total_down;
    }
    return false;
}
