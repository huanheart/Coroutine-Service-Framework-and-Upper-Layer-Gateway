#ifndef GATEWAY_METRICS_H
#define GATEWAY_METRICS_H

#include <atomic>
#include <string>

class GatewayMetrics {
public:
    static GatewayMetrics& instance();

    void on_request();
    void on_upstream_error();
    void on_filter_reject();

    std::string render_plain() const;

private:
    GatewayMetrics();

private:
    std::atomic<unsigned long long> m_total_requests;
    std::atomic<unsigned long long> m_upstream_errors;
    std::atomic<unsigned long long> m_filter_rejects;
};

#endif

