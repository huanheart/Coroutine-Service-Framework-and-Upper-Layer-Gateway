#include "../server/tcp_server.h"
#include "../util/socket.h"
#include "../util/address.h"
#include "../CoroutineLibrary/ioscheduler.h"
#include <string>
#include <iostream>

class UpstreamEchoServer : public sylar::TcpServer {
public:
    UpstreamEchoServer(sylar::IOManager* worker = sylar::IOManager::GetThis(),
                       sylar::IOManager* io_worker = sylar::IOManager::GetThis(),
                       sylar::IOManager* accept_worker = sylar::IOManager::GetThis())
        : sylar::TcpServer(worker, io_worker, accept_worker) {}

    void handleClient(std::shared_ptr<sylar::Socket> client) override {
        char buf[4096];
        std::string request;
        while (true) {
            int n = client->recv(buf, sizeof(buf), 0);
            if (n <= 0) break;
            request.append(buf, n);
            if (request.find("\r\n\r\n") != std::string::npos) break;
        }
        if (request.empty()) { client->close(); return; }
        size_t eol = request.find("\r\n");
        std::string line = eol == std::string::npos ? request : request.substr(0, eol);
        std::string method = "GET", path = "/", version = "HTTP/1.1";
        {
            size_t p1 = line.find(' ');
            size_t p2 = line.find(' ', p1 == std::string::npos ? 0 : p1 + 1);
            if (p1 != std::string::npos && p2 != std::string::npos) {
                method = line.substr(0, p1);
                path = line.substr(p1 + 1, p2 - p1 - 1);
                version = line.substr(p2 + 1);
            }
        }
        if (method == "HEAD") {
            std::string headers = version + " 200 OK\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
            client->send(headers.data(), headers.size(), 0);
            client->close();
            return;
        }
        std::string body = "echo " + path + "\n";
        std::string headers = version + " 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n";
        std::string data = headers + body;
        client->send(data.data(), data.size(), 0);
        client->close();
    }
};

int main() {
    sylar::IOManager manager(4, true);
    manager.scheduleLock([&]() {
        std::vector<std::string> ports = {"8000", "8001", "8002", "8080", "9000"};
        std::vector<std::shared_ptr<UpstreamEchoServer>> servers;
        for (auto& p : ports) {
            sylar::Address::ptr addr = sylar::Address::LookupAnyIPAddress("0.0.0.0:" + p);
            auto s = std::make_shared<UpstreamEchoServer>();
            if (!s->bind(addr, false)) {
                std::cout << "bind fail " << p << std::endl;
                continue;
            }
            s->start();
            servers.push_back(s);
            std::cout << "upstream listen " << p << std::endl;
        }
    });
    return 0;
}
