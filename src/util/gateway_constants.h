#ifndef GATEWAY_CONSTANTS_H
#define GATEWAY_CONSTANTS_H
#include <string>
namespace GatewayConst {
static constexpr const char* ADMIN_METRICS_PATH = "/__admin/metrics";
static constexpr const char* ADMIN_RELOAD_PATH = "/__admin/reload";
static constexpr const char* CFG_ROUTE = "route";
static constexpr const char* CFG_EXACT = "exact";
static constexpr const char* CFG_BLACK = "black";
static constexpr const char* CFG_QPS = "qps";
static constexpr const char* CFG_TOKEN = "token";
static constexpr const char* CFG_MAX_BODY_SIZE = "max_body_size";
static constexpr const char* CFG_TIMEOUT_CONNECT_MS = "timeout_connect_ms";
static constexpr const char* CFG_TIMEOUT_SEND_MS = "timeout_send_ms";
static constexpr const char* CFG_TIMEOUT_RECV_MS = "timeout_recv_ms";
static constexpr const char* CFG_HEARTBEAT_INTERVAL_MS = "heartbeat_interval_ms";
static constexpr const char* CFG_HEARTBEAT_PATH = "heartbeat_path";
static constexpr const char* CFG_ENABLE_HEARTBEAT = "enable_heartbeat";
static constexpr const char* CFG_IDLE_DISCONNECT_MS = "idle_disconnect_ms";
static constexpr const char* CFG_JWT_ENABLED = "jwt_enabled";
static constexpr const char* CFG_JWT_SECRET = "jwt_secret";
static constexpr const char* CFG_JWT_ISSUER = "jwt_issuer";
static constexpr const char* CFG_JWT_AUDIENCE = "jwt_audience";
static constexpr const char* CFG_JWT_ALG = "jwt_alg";
static constexpr const char* CFG_JWT_CACHE_CAPACITY = "jwt_cache_capacity";
struct HttpStatus {
    static constexpr int OK = 200;
    static constexpr int BAD_REQUEST = 400;
    static constexpr int UNAUTHORIZED = 401;
    static constexpr int FORBIDDEN = 403;
    static constexpr int METHOD_NOT_ALLOWED = 405;
    static constexpr int PAYLOAD_TOO_LARGE = 413;
    static constexpr int TOO_MANY_REQUESTS = 429;
    static constexpr int INTERNAL_ERROR = 500;
    static constexpr int BAD_GATEWAY = 502;
};
inline std::string status_text(int code) {
    switch (code) {
        case HttpStatus::OK: return "OK";
        case HttpStatus::BAD_REQUEST: return "Bad Request";
        case HttpStatus::UNAUTHORIZED: return "Unauthorized";
        case HttpStatus::FORBIDDEN: return "Forbidden";
        case HttpStatus::METHOD_NOT_ALLOWED: return "Method Not Allowed";
        case HttpStatus::PAYLOAD_TOO_LARGE: return "Payload Too Large";
        case HttpStatus::TOO_MANY_REQUESTS: return "Too Many Requests";
        case HttpStatus::INTERNAL_ERROR: return "Internal Server Error";
        case HttpStatus::BAD_GATEWAY: return "Bad Gateway";
        default: return "Error";
    }
}
}
#endif
