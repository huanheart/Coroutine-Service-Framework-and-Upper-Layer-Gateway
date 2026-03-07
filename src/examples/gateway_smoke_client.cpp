#include "../util/socket.h"
#include "../util/address.h"
#include "../util/jwt_utils.h"
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <vector>
#include <unistd.h>

struct JwtConf {
    std::string secret;
    std::string issuer;
    std::string audience;
    int64_t max_body_size{0};
};

static bool file_exists(const std::string& p) {
    return access(p.c_str(), F_OK) == 0;
}

static std::string resolve_conf_path(const std::string& hint) {
    if (file_exists(hint)) return hint;
    std::vector<std::string> tries = {
        "conf/gateway.conf",
        "../conf/gateway.conf",
        "../../conf/gateway.conf"
    };
    for (auto& t : tries) {
        if (file_exists(t)) return t;
    }
    return hint;
}

JwtConf read_conf(const std::string& path_hint) {
    std::string path = resolve_conf_path(path_hint);
    JwtConf c;
    std::ifstream in(path.c_str());
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string t, v;
        ss >> t >> v;
        if (t == "jwt_secret") c.secret = v;
        else if (t == "jwt_issuer") c.issuer = v;
        else if (t == "jwt_audience") c.audience = v;
        else if (t == "max_body_size") { c.max_body_size = std::strtoll(v.c_str(), nullptr, 10); }
    }
    return c;
}

std::string build_req(const std::string& method, const std::string& path, const std::string& host, const std::string& auth) {
    std::ostringstream ss;
    ss << method << " " << path << " HTTP/1.1\r\n"
       << "Host: " << host << "\r\n"
       << "Connection: close\r\n";
    if (!auth.empty()) {
        ss << "Authorization: Bearer " << auth << "\r\n";
    }
    ss << "Content-Length: 0\r\n\r\n";
    return ss.str();
}

std::string build_req_with_body(const std::string& method, const std::string& path, const std::string& host, const std::string& auth, const std::string& body) {
    std::ostringstream ss;
    ss << method << " " << path << " HTTP/1.1\r\n"
       << "Host: " << host << "\r\n"
       << "Connection: close\r\n";
    if (!auth.empty()) {
        ss << "Authorization: Bearer " << auth << "\r\n";
    }
    ss << "Content-Type: text/plain\r\n"
       << "Content-Length: " << body.size() << "\r\n\r\n"
       << body;
    return ss.str();
}

int main(int argc, char** argv) {
    std::string gw = argc > 1 ? argv[1] : "127.0.0.1:9006";
    std::string conf = argc > 2 ? argv[2] : "../conf/gateway.conf";
    JwtConf jc = read_conf(conf);
    int64_t exp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count() + 300;
    std::string token = jwt::sign_hs256(jc.secret, jc.issuer, jc.audience, exp);
    std::string bad_secret_token = jwt::sign_hs256("wrong-secret", jc.issuer, jc.audience, exp);
    std::string bad_issuer_token = jwt::sign_hs256(jc.secret, "wrong-issuer", jc.audience, exp);
    std::string bad_audience_token = jwt::sign_hs256(jc.secret, jc.issuer, "wrong-audience", exp);
    std::string expired_token = jwt::sign_hs256(jc.secret, jc.issuer, jc.audience, exp - 3600);

    std::string host = gw;
    auto send_req = [&](const std::string& method, const std::string& path, const std::string& auth) {
        sylar::Address::ptr addr = sylar::Address::LookupAnyIPAddress(gw);
        auto sock = sylar::Socket::CreateTCP(addr);
        if (!sock->connect(addr, 2000)) {
            std::cout << "connect gateway fail\n";
            return;
        }
        std::string req = build_req(method, path, host, auth);
        int sent = sock->send(req.data(), req.size(), 0);
        if (sent <= 0) {
            std::cout << "send fail\n";
            return;
        }
        char buf[4096];
        std::string resp;
        while (true) {
            int n = sock->recv(buf, sizeof(buf), 0);
            if (n <= 0) break;
            resp.append(buf, n);
        }
        std::cout << "=== " << method << " " << path << " ===\n" << resp << "\n";
    };
    auto send_req_body = [&](const std::string& method, const std::string& path, const std::string& auth, const std::string& body) {
        sylar::Address::ptr addr = sylar::Address::LookupAnyIPAddress(gw);
        auto sock = sylar::Socket::CreateTCP(addr);
        if (!sock->connect(addr, 2000)) {
            std::cout << "connect gateway fail\n";
            return;
        }
        std::string req = build_req_with_body(method, path, host, auth, body);
        int sent = sock->send(req.data(), req.size(), 0);
        if (sent <= 0) {
            std::cout << "send fail\n";
            return;
        }
        char buf[4096];
        std::string resp;
        while (true) {
            int n = sock->recv(buf, sizeof(buf), 0);
            if (n <= 0) break;
            resp.append(buf, n);
        }
        std::cout << "=== " << method << " " << path << " (body " << body.size() << ") ===\n" << resp << "\n";
    };

    send_req("GET", "/__admin/metrics", "");
    std::cout << "token(len=" << token.size() << ")\n";
    send_req("GET", "/service1/echo", token);
    //发送第二次,测试缓存是否命中
    send_req("GET", "/service1/echo", token);
    
    send_req("GET", "/service1/echo", "");
    send_req("GET", "/service1/echo", bad_secret_token);
    send_req("GET", "/service1/echo", bad_issuer_token);
    send_req("GET", "/service1/echo", bad_audience_token);
    send_req("GET", "/service1/echo", expired_token);
    send_req("GET", "/echo", token);
    if (jc.max_body_size > 0) {
        std::string big(jc.max_body_size + 1, 'x');
        send_req_body("POST", "/service1/echo", token, big);
    }
    send_req("GET", "/__admin/reload", "");
    return 0;
}
