#include "gateway_server.h"
#include "../CoroutineLibrary/hook.h"
#include "../util/config.h"
#include "../util/address.h"
#include "../gateway/gateway_router.h"
#include "../gateway/gateway_filter_chain.h"
#include "../gateway/gateway_metrics.h"
#include "../gateway/gateway_config.h"
#include "gateway_conn.h"
#include <muduo/base/Logging.h>
#include <unistd.h>

using namespace std;

namespace {
const int GW_MAX_FD = 65536;
gateway_conn* g_conns = nullptr;
}

gateway_server::gateway_server(sylar::IOManager* worker,
                               sylar::IOManager* io_worker,
                               sylar::IOManager* accept_worker)
    : sylar::TcpServer(worker, io_worker, accept_worker) {
    if (access("../../conf/gateway.conf", F_OK) == 0) {
        GatewayConfig::instance().set_config_path("../../conf/gateway.conf");
        bool ok = GatewayConfig::instance().load_from_file("../../conf/gateway.conf");
        if (Config::get_instance()->get_close_log() == 0) {
            LOG_INFO << "gateway config load ../../conf/gateway.conf ok=" << ok;
        }
    } else if (access("../conf/gateway.conf", F_OK) == 0) {
        GatewayConfig::instance().set_config_path("../conf/gateway.conf");
        bool ok = GatewayConfig::instance().load_from_file("../conf/gateway.conf");
        if (Config::get_instance()->get_close_log() == 0) {
            LOG_INFO << "gateway config load ../conf/gateway.conf ok=" << ok;
        }
    } else {
        if (Config::get_instance()->get_close_log() == 0) {
            LOG_INFO << "gateway config not found at ../conf or ../../conf";
        }
    }
}

gateway_server::~gateway_server() {
}

void gateway_server::handleClient(sylar::Socket::ptr client) {
    if (Config::get_instance()->get_close_log() == 0) {
        LOG_INFO << "gateway_server handleClient and dealing " << client->getSocket() << " now ";
    }
    sylar::set_hook_enable(true);
    if (!client->isValid()) {
        client->close();
        return;
    }
    if (!g_conns) {
        g_conns = new gateway_conn[GW_MAX_FD];
    }
    int client_socket = client->getSocket();
    g_conns[client_socket].init(client);
    while (client->isConnected()) {
        if (!g_conns[client_socket].read_once()) {
            break;
        }
        if (!g_conns[client_socket].process()) {
            break;
        }
    }
    std::cout<<"gateway_server handleClient and close " << client->getSocket() << " now "<<std::endl;
    client->close();
}
