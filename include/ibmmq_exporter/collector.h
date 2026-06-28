#pragma once

#include <atomic>
#include <chrono>
#include <memory>

#include <prometheus/registry.h>

#include "ibmmq_exporter/config.h"
#include "ibmmq_exporter/http_server.h"
#include "ibmmq_exporter/metrics_collector.h"
#include "ibmmq_exporter/mqclient.h"
#include "ibmmq_exporter/resource_monitor.h"

namespace ibmmq_exporter {

class Collector {
public:
    explicit Collector(const Config& config);
    ~Collector();

    Collector(const Collector&) = delete;
    Collector& operator=(const Collector&) = delete;

    // Start collection (blocks until stopped or max cycles reached)
    void start();

    // Request graceful stop (thread-safe)
    void stop();

    [[nodiscard]] bool is_running() const { return running_.load(); }

private:
    void run_once();
    void run_continuous();
    void collect_cycle();
    bool try_connect();
    void setup_after_connect();

    Config                                config_;
    std::unique_ptr<MQClient>             mq_client_;
    std::shared_ptr<prometheus::Registry> registry_;
    std::unique_ptr<MetricsCollector>     metrics_collector_;
    std::unique_ptr<HTTPServer>           http_server_;
    std::unique_ptr<ResourceMonitor>     resource_monitor_;

    std::atomic<bool> running_{false};
    int               cycle_count_{0};
    int64_t           total_collections_{0};
    int64_t           error_count_{0};

    // Reconnection state (matching Go status.go)
    std::atomic<bool> connected_once_{false};
    std::atomic<bool> connected_qmgr_{false};
    std::atomic<bool> collector_end_{false};
    std::atomic<bool> first_collection_{false};

    // Rediscovery timer
    std::chrono::steady_clock::time_point last_rediscovery_;
    std::vector<std::string> discovered_queues_;
};

} // namespace ibmmq_exporter
