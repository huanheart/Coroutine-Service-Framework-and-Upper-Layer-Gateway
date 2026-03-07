#include "jwt_cache.h"

JwtCache& JwtCache::instance() {
    static JwtCache c;
    return c;
}

JwtCache::JwtCache()
    : m_cache(10000) {
}

bool JwtCache::is_valid_cached(const std::string& token, int64_t now_sec) const {
    auto p = m_cache.get(token);
    if (!p.first) return false;
    return p.second > now_sec;
}

void JwtCache::put_valid(const std::string& token, int64_t exp_sec) {
    m_cache.push(token, exp_sec);
}

void JwtCache::clear() {
    m_cache = LRUCache<std::string, int64_t>(10000);
}

void JwtCache::set_capacity(int cap) {
    m_cache = LRUCache<std::string, int64_t>(cap);
}
