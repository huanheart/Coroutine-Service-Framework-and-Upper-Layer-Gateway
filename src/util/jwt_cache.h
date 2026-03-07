#ifndef JWT_CACHE_H
#define JWT_CACHE_H
#include <string>
#include <utility>
#include "../my_stl/my_stl.hpp"

class JwtCache {
public:
    static JwtCache& instance();
    bool is_valid_cached(const std::string& token, int64_t now_sec) const;
    void put_valid(const std::string& token, int64_t exp_sec);
    void clear();
    void set_capacity(int cap);
private:
    JwtCache();
    mutable LRUCache<std::string, int64_t> m_cache;
};

#endif
