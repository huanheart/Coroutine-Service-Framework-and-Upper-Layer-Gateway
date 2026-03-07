#include "gateway_filter_chain.h"

#include "gateway_config.h"
#include "../util/gateway_constants.h"
#include "../util/jwt_utils.h"
#include "../util/jwt_cache.h"

#include <algorithm>
#include <ctime>

class BlacklistFilter : public GatewayFilter {
public:
    bool apply(GatewayRequestContext& ctx, int& status, std::string& body) override {
        GatewayConfig& cfg = GatewayConfig::instance();
        const auto& list = cfg.blacklist();
        if (list.empty()) {
            return true;
        }
        if (ctx.client_address.empty()) {
            return true;
        }
        auto it = std::find(list.begin(), list.end(), ctx.client_address);
        if (it != list.end()) {
            status = GatewayConst::HttpStatus::FORBIDDEN;
            body = "blocked\n";
            return false;
        }
        return true;
    }
};

class RateLimitFilter : public GatewayFilter {
public:
    RateLimitFilter()
        : m_last_sec(0),
          m_count(0) {
    }

    bool apply(GatewayRequestContext& ctx, int& status, std::string& body) override {
        GatewayConfig& cfg = GatewayConfig::instance();
        int limit = cfg.max_qps();
        if (limit <= 0) {
            return true;
        }
        std::time_t now = std::time(nullptr);
        if (now != m_last_sec) {
            m_last_sec = now;
            m_count = 0;
        }
        m_count++;
        if (m_count > limit) {
            status = GatewayConst::HttpStatus::TOO_MANY_REQUESTS;
            body = "too many requests\n";
            return false;
        }
        return true;
    }

private:
    std::time_t m_last_sec;
    int m_count;
};

class AuthFilter : public GatewayFilter {
public:
    bool apply(GatewayRequestContext& ctx, int& status, std::string& body) override {
        GatewayConfig& cfg = GatewayConfig::instance();
        if (cfg.jwt_enabled()) {
            size_t pos = ctx.raw_request.find("\r\n\r\n");
            std::string headers = ctx.raw_request.substr(0, pos);
            std::string key = "Authorization:";
            size_t h = headers.find(key);
            if (h == std::string::npos) {
                status = GatewayConst::HttpStatus::UNAUTHORIZED;
                body = "unauthorized\n";
                return false;
            }
            size_t end = headers.find("\r\n", h);
            std::string line = headers.substr(h + key.size(), end == std::string::npos ? std::string::npos : (end - h - key.size()));
            size_t s = line.find_first_not_of(" \t");
            size_t e = line.find_last_not_of(" \t");
            std::string val = (s == std::string::npos) ? "" : line.substr(s, e == std::string::npos ? std::string::npos : (e - s + 1));
            std::string be = "Bearer ";
            std::string token;
            if (val.rfind(be, 0) == 0) {
                token = val.substr(be.size());
            } else {
                token = val;
            }
            int64_t now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            if (JwtCache::instance().is_valid_cached(token, now)) {
                return true;
            }
            std::string err;
            if (!jwt::verify_hs256(token, cfg.jwt_secret(), cfg.jwt_issuer(), cfg.jwt_audience(), err)) {
                status = GatewayConst::HttpStatus::UNAUTHORIZED;
                body = "unauthorized\n";
                return false;
            }
            int64_t exp = 0;
            jwt::get_exp(token, exp);
            if (exp > 0) {
                JwtCache::instance().put_valid(token, exp);
            }
            return true;
        }
        const RouteConfig* rc = cfg.match_route(ctx.path);
        if (!rc || rc->required_token.empty()) {
            return true;
        }
        size_t pos = ctx.raw_request.find("\r\n\r\n");
        std::string headers = ctx.raw_request.substr(0, pos);
        std::string key = "Authorization:";
        size_t h = headers.find(key);
        if (h == std::string::npos) {
            status = GatewayConst::HttpStatus::UNAUTHORIZED;
            body = "unauthorized\n";
            return false;
        }
        size_t end = headers.find("\r\n", h);
        std::string line = headers.substr(h + key.size(), end == std::string::npos ? std::string::npos : (end - h - key.size()));
        size_t s = line.find_first_not_of(" \t");
        size_t e = line.find_last_not_of(" \t");
        std::string val = (s == std::string::npos) ? "" : line.substr(s, e == std::string::npos ? std::string::npos : (e - s + 1));
        std::string be = "Bearer ";
        std::string client_token;
        if (val.rfind(be, 0) == 0) {
            client_token = val.substr(be.size());
        } else {
            client_token = val;
        }
        if (client_token != rc->required_token) {
            status = GatewayConst::HttpStatus::UNAUTHORIZED;
            body = "unauthorized\n";
            return false;
        }
        return true;
    }
};

GatewayFilterChain& GatewayFilterChain::instance() {
    static GatewayFilterChain c;
    if (!c.m_inited) {
        c.init_default();
    }
    return c;
}

GatewayFilterChain::GatewayFilterChain()
    : m_inited(false) {
}

void GatewayFilterChain::init_default() {
    if (m_inited) {
        return;
    }
    m_filters.clear();
    m_filters.emplace_back(new BlacklistFilter());
    m_filters.emplace_back(new RateLimitFilter());
    m_filters.emplace_back(new AuthFilter());
    m_inited = true;
}

bool GatewayFilterChain::apply(GatewayRequestContext& ctx, int& status, std::string& body) {
    for (auto& f : m_filters) {
        if (!f->apply(ctx, status, body)) {
            return false;
        }
    }
    return true;
}
