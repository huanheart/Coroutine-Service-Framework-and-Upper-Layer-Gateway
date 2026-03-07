#include "jwt_cache.h"
#include <muduo/base/Logging.h>
#include "config.h"

JwtCache& JwtCache::instance() {
    static JwtCache c;
    return c;
}

JwtCache::JwtCache()
    : m_cache(10000) {
}

bool JwtCache::is_valid_cached(const std::string& token, int64_t now_sec) const {
    auto p = m_cache.get(token);
    if (!p.first) {
        if (Config::get_instance()->get_close_log() == 0) {
            LOG_INFO << "jwt cache miss len=" << token.size();
        }
        return false;
    }
    bool ok = p.second > now_sec;
    if (Config::get_instance()->get_close_log() == 0) {
        LOG_INFO << "jwt cache hit len=" << token.size() << " exp=" << p.second << " now=" << now_sec << " ok=" << ok;
    }
    return ok;
}

void JwtCache::put_valid(const std::string& token, int64_t exp_sec) {
    m_cache.push(token, exp_sec);
    if (Config::get_instance()->get_close_log() == 0) {
        LOG_INFO << "jwt cache put len=" << token.size() << " exp=" << exp_sec;
    }
}

void JwtCache::clear() {
    m_cache = LRUCache<std::string, int64_t>(10000);
    if (Config::get_instance()->get_close_log() == 0) {
        LOG_INFO << "jwt cache clear";
    }
}

void JwtCache::set_capacity(int cap) {
    m_cache = LRUCache<std::string, int64_t>(cap);
    if (Config::get_instance()->get_close_log() == 0) {
        LOG_INFO << "jwt cache set capacity=" << cap;
    }
}
