#ifndef GATEWAY_SERVER_H
#define GATEWAY_SERVER_H

#include <string>
#include "tcp_server.h"
#include "../util/socket.h"

class gateway_server : public sylar::TcpServer {
public:
    gateway_server(sylar::IOManager* worker = sylar::IOManager::GetThis(),
                   sylar::IOManager* io_worker = sylar::IOManager::GetThis(),
                   sylar::IOManager* accept_worker = sylar::IOManager::GetThis());
    ~gateway_server();

    void handleClient(std::shared_ptr<sylar::Socket> client) override;
};

#endif

