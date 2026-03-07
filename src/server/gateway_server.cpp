#include "gateway_server.h"
#include "../CoroutineLibrary/hook.h"
#include "../util/config.h"
#include "../util/address.h"
#include "../gateway/gateway_router.h"
#include "../gateway/gateway_filter_chain.h"
#include "../gateway/gateway_metrics.h"
#include "../gateway/gateway_config.h"
#include "../gateway/gateway_health.h"
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
        GatewayConfig::instance().load_from_file("../../conf/gateway.conf");
    } else if (access("../conf/gateway.conf", F_OK) == 0) {
        GatewayConfig::instance().set_config_path("../conf/gateway.conf");
        GatewayConfig::instance().load_from_file("../conf/gateway.conf");
    }
    GatewayHealthManager::instance().start();
    sylar::IOManager* scheduler = sylar::IOManager::GetThis();
    if (scheduler) {
        int prune_interval = GatewayConfig::instance().idle_disconnect_ms() / 2;
        if (prune_interval <= 0) prune_interval = 1000;
        m_idle_prune_timer = scheduler->addTimer(prune_interval, []() {
            if (!g_conns) return;
            for (int i = 0; i < GW_MAX_FD; ++i) {
                g_conns[i].prune_idle_upstreams();
            }
        }, true);
        if (Config::get_instance()->get_close_log() == 0) {
            LOG_INFO << "gateway global idle prune started interval_ms=" << prune_interval;
        }
    }
}

gateway_server::~gateway_server() {
    if (m_idle_prune_timer) {
        m_idle_prune_timer->cancel();
        m_idle_prune_timer.reset();
    }
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
    client->close();
}
