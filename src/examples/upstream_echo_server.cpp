#include "../server/tcp_server.h"
#include "../util/socket.h"
#include "../util/address.h"
#include "../CoroutineLibrary/ioscheduler.h"
#include <string>
#include <iostream>
#include <muduo/base/Logging.h>
#include <atomic>
#include <chrono>

class UpstreamEchoServer : public sylar::TcpServer {
public:
    UpstreamEchoServer(sylar::IOManager* worker = sylar::IOManager::GetThis(),
                       sylar::IOManager* io_worker = sylar::IOManager::GetThis(),
                       sylar::IOManager* accept_worker = sylar::IOManager::GetThis())
        : sylar::TcpServer(worker, io_worker, accept_worker) {}
private:
 

    void handleClient(std::shared_ptr<sylar::Socket> client) override {
        char buf[4096];
        std::string local_str;
        {
            auto local = client->getLocalAddress();
            if (local) local_str = local->toString();
        }
        while (true) {
            std::string request;
            while (true) {
                int n = client->recv(buf, sizeof(buf), 0);
                if (n <= 0) { 
                    std::cout<<"upstream_server handleClient and close " << client->getSocket() << " now "<<std::endl;
                    client->close(); 
                    return; 
                }
                request.append(buf, n);
                if (request.find("\r\n\r\n") != std::string::npos) break;
            }
            LOG_INFO << "upstream " << local_str << " recv bytes=" << request.size();
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
            LOG_INFO << "upstream " << local_str << " request " << method << " " << path << " " << version;
            bool close_conn = false;
            {
                size_t hdr_start = eol == std::string::npos ? std::string::npos : eol + 2;
                if (hdr_start != std::string::npos) {
                    std::string headers_block = request.substr(hdr_start, request.find("\r\n\r\n") - hdr_start);
                    std::string lower = headers_block;
                    for (auto& c : lower) c = ::tolower(c);
                    if (lower.find("connection: close") != std::string::npos) close_conn = true;
                }
                if (version == "HTTP/1.0") {
                    close_conn = true;
                }
            }
            std::string body = "echo " + path + "\n";
            std::string headers = version + " 200 OK\r\nContent-Type: text/plain\r\nConnection: " + std::string(close_conn ? "close" : "keep-alive") + "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n";
            std::string data = headers + body;
            client->send(data.data(), data.size(), 0);
            LOG_INFO << "upstream " << local_str << " send bytes=" << data.size();
            if (close_conn) {
                std::cout<<"upstream_server handleClient and close " << client->getSocket() << " now "<<std::endl;
                client->close();
                return;
            }
        }
    }
};

int main() {
    sylar::IOManager manager(4, true);
    manager.scheduleLock([&]() {
        std::vector<std::string> ports = {"8001", "8002","9000","8080"};
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
            LOG_INFO << "upstream listen " << p;
        }
    });
    return 0;
}
