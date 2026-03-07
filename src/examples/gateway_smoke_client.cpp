#include "../util/socket.h"
#include "../util/address.h"
#include "../util/jwt_utils.h"
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>

struct JwtConf {
    std::string secret;
    std::string issuer;
    std::string audience;
};

JwtConf read_conf(const std::string& path) {
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

int main(int argc, char** argv) {
    std::string gw = argc > 1 ? argv[1] : "127.0.0.1:9006";
    std::string conf = argc > 2 ? argv[2] : "conf/gateway.conf";
    JwtConf jc = read_conf(conf);
    int64_t exp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count() + 300;
    std::string token = jwt::sign_hs256(jc.secret, jc.issuer, jc.audience, exp);

    sylar::Address::ptr addr = sylar::Address::LookupAnyIPAddress(gw);
    auto sock = sylar::Socket::CreateTCP(addr);
    if (!sock->connect(addr, 2000)) {
        std::cout << "connect gateway fail\n";
        return 1;
    }
    std::string host = gw;
    auto send_req = [&](const std::string& method, const std::string& path, const std::string& auth) {
        std::string req = build_req(method, path, host, auth);
        sock->send(req.data(), req.size(), 0);
        char buf[4096];
        std::string resp;
        while (true) {
            int n = sock->recv(buf, sizeof(buf), 0);
            if (n <= 0) break;
            resp.append(buf, n);
        }
        std::cout << "=== " << method << " " << path << " ===\n" << resp << "\n";
    };

    send_req("GET", "/__admin/metrics", "");
    send_req("GET", "/service1/echo", token);
    send_req("GET", "/echo", token);
    return 0;
}
