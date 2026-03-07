#ifndef GATEWAY_CONN_H
#define GATEWAY_CONN_H

#include <string>
#include <unordered_map>
#include "../CoroutineLibrary/ioscheduler.h"
#include "../util/socket.h"

class gateway_conn {
public:
    static const int READ_BUFFER_SIZE = 16 * 1024;

    gateway_conn() = default;
    ~gateway_conn() = default;

    void init(sylar::Socket::ptr& client);
    bool read_once();
    bool process();

private:
    bool parse_request_line(const std::string& request, std::string& method, std::string& path, std::string& version);
    void send_simple_response(int status, const std::string& status_text, const std::string& body);
    sylar::Socket::ptr get_upstream_socket(const std::string& path);
    void start_heartbeat(const std::string& key);

private:
    sylar::Socket::ptr m_client;
    char m_read_buf[READ_BUFFER_SIZE];
    size_t m_read_idx = 0;
    std::string m_request;
    long m_content_length = 0;
    struct UpstreamEntry {
        sylar::Socket::ptr sock;
        std::shared_ptr<sylar::Timer> timer;
        bool busy = false;
        uint64_t last_used_ms = 0;
    };
    std::unordered_map<std::string, UpstreamEntry> m_upstreams;
    std::string m_curr_upstream_key;
    bool m_oversize = false;
};

#endif
