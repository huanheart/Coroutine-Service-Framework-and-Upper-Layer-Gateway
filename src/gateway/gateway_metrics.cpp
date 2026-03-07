#include "gateway_metrics.h"

#include <sstream>

GatewayMetrics& GatewayMetrics::instance() {
    static GatewayMetrics m;
    return m;
}

GatewayMetrics::GatewayMetrics()
    : m_total_requests(0),
      m_upstream_errors(0),
      m_filter_rejects(0) {
}

void GatewayMetrics::on_request() {
    m_total_requests.fetch_add(1, std::memory_order_relaxed);
}

void GatewayMetrics::on_upstream_error() {
    m_upstream_errors.fetch_add(1, std::memory_order_relaxed);
}

void GatewayMetrics::on_filter_reject() {
    m_filter_rejects.fetch_add(1, std::memory_order_relaxed);
}

std::string GatewayMetrics::render_plain() const {
    std::ostringstream ss;
    ss << "total_requests " << m_total_requests.load(std::memory_order_relaxed) << "\n";
    ss << "upstream_errors " << m_upstream_errors.load(std::memory_order_relaxed) << "\n";
    ss << "filter_rejects " << m_filter_rejects.load(std::memory_order_relaxed) << "\n";
    return ss.str();
}

