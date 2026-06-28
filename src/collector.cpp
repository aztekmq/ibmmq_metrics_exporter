#include "ibmmq_exporter/collector.h"

#include <chrono>
#include <thread>

#include <spdlog/spdlog.h>

namespace ibmmq_exporter {

Collector::Collector(const Config& config) : config_(config) {
    mq_client_ = std::make_unique<MQClient>(config_.mq);
    registry_  = std::make_shared<prometheus::Registry>();
    metrics_collector_ = std::make_unique<MetricsCollector>(config_, *mq_client_, registry_);
    http_server_ = std::make_unique<HTTPServer>(config_.prometheus, registry_);

    spdlog::info("Created IBM MQ statistics collector: QM={}, Channel={}",
                 config_.mq.queue_manager, config_.mq.channel);
}

Collector::~Collector() {
    stop();
}

bool Collector::try_connect() {
    try {
        mq_client_->connect();
        connected_qmgr_ = true;
        connected_once_ = true;
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to connect to queue manager: {}", e.what());
        connected_qmgr_ = false;
        return false;
    }
}

void Collector::setup_after_connect() {
    // Open queues
    try {
        mq_client_->open_stats_queue(config_.collector.stats_queue);
    } catch (const std::exception& e) {
        spdlog::warn("Failed to open statistics queue: {}", e.what());
    }

    try {
        mq_client_->open_accounting_queue(config_.collector.accounting_queue);
    } catch (const std::exception& e) {
        spdlog::warn("Failed to open accounting queue: {}", e.what());
    }

    // Subscribe to system topics if configured
    if (!config_.collector.monitored_topics.empty()) {
        for (const auto& topic : config_.collector.monitored_topics) {
            mq_client_->subscribe_to_topic(topic);
        }
    }

    // Queue discovery (before resource monitor — per-queue publication
    // subscriptions need the queue list)
    if (!config_.collector.monitored_queues.empty()) {
        for (const auto& pattern : config_.collector.monitored_queues) {
            auto qs = mq_client_->discover_queues(pattern);
            discovered_queues_.insert(discovered_queues_.end(), qs.begin(), qs.end());
        }
    }

    // Set up resource monitor for $SYS topic publication metrics
    if (config_.collector.use_publications) {
        try {
            resource_monitor_ = std::make_unique<ResourceMonitor>(*mq_client_, config_.mq.queue_manager);
            if (resource_monitor_->discover()) {
                resource_monitor_->create_subscriptions(discovered_queues_);
                metrics_collector_->set_resource_monitor(resource_monitor_.get());
                spdlog::info("Resource monitor initialized with {} classes", resource_monitor_->class_count());
            } else {
                spdlog::warn("Resource monitor discovery failed, publication metrics disabled");
                resource_monitor_.reset();
            }
        } catch (const std::exception& e) {
            spdlog::warn("Failed to initialize resource monitor: {}", e.what());
            resource_monitor_.reset();
        }
    }

    last_rediscovery_ = std::chrono::steady_clock::now();
}

void Collector::start() {
    if (running_.load()) {
        spdlog::warn("Collector is already running");
        return;
    }

    spdlog::info("Starting IBM MQ statistics collector");

    // Start HTTP server first (so /metrics is reachable even when disconnected)
    http_server_->start();
    running_ = true;

    if (config_.collector.keep_running) {
        // Reconnection loop
        while (running_.load() && !collector_end_.load()) {
            if (!connected_qmgr_.load()) {
                if (try_connect()) {
                    setup_after_connect();
                } else {
                    if (!connected_once_.load() && !config_.collector.keep_running) {
                        spdlog::error("Cannot connect and keep_running is false, exiting");
                        collector_end_ = true;
                        break;
                    }
                    spdlog::info("Retrying connection in {}s", config_.collector.reconnect_interval.count());
                    for (int i = 0; i < static_cast<int>(config_.collector.reconnect_interval.count()) * 10; ++i) {
                        if (!running_.load()) break;
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                    continue;
                }
            }

            // Connected: run collection
            if (config_.collector.continuous) {
                run_continuous();
            } else {
                run_once();
                collector_end_ = true;
            }

            // If we exit the collection loop but keep_running is true,
            // it means we lost the connection; loop around to reconnect
            if (running_.load() && !collector_end_.load()) {
                connected_qmgr_ = false;
                try {
                    mq_client_->disconnect();
                } catch (...) {}
                resource_monitor_.reset();
                metrics_collector_->set_resource_monitor(nullptr);
                mq_client_ = std::make_unique<MQClient>(config_.mq);
                metrics_collector_->set_mq_client(*mq_client_);
                spdlog::warn("Connection lost, will attempt reconnection");
            }
        }
    } else {
        // No keep_running: connect once or fail
        if (!try_connect()) {
            spdlog::error("Failed to connect and keep_running is disabled");
            running_ = false;
            return;
        }
        setup_after_connect();

        if (config_.collector.continuous) {
            run_continuous();
        } else {
            run_once();
        }
    }
}

void Collector::stop() {
    if (!running_.load()) return;

    spdlog::info("Stopping IBM MQ statistics collector");
    running_ = false;
    collector_end_ = true;

    http_server_->stop();

    // Close resource monitor BEFORE disconnecting MQClient.
    // This clears internal state so the metrics collector won't try to use it.
    // The actual managed subscription cleanup (draining messages + unsubscribe)
    // happens in MQClient::disconnect() → unsubscribe_all().
    if (resource_monitor_) {
        metrics_collector_->set_resource_monitor(nullptr);
        resource_monitor_->close();
        resource_monitor_.reset();
    }

    try {
        mq_client_->disconnect();
    } catch (const std::exception& e) {
        spdlog::error("Error disconnecting: {}", e.what());
    }

    spdlog::info("Collector stopped: collections={}, errors={}",
                 total_collections_, error_count_);
}

void Collector::run_once() {
    spdlog::info("Running single collection cycle");
    collect_cycle();
}

void Collector::run_continuous() {
    spdlog::info("Starting continuous collection: interval={}s, max_cycles={}",
                 config_.collector.interval.count(), config_.collector.max_cycles);

    // Initial collection
    collect_cycle();

    while (running_.load()) {
        // Interruptible sleep
        for (int i = 0; i < static_cast<int>(config_.collector.interval.count()) * 10; ++i) {
            if (!running_.load()) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (!running_.load()) break;

        // Check for rediscovery
        if (config_.collector.rediscover_interval.count() > 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_rediscovery_);
            if (elapsed >= config_.collector.rediscover_interval) {
                spdlog::info("Running queue rediscovery");
                discovered_queues_.clear();
                for (const auto& pattern : config_.collector.monitored_queues) {
                    auto qs = mq_client_->discover_queues(pattern);
                    discovered_queues_.insert(discovered_queues_.end(), qs.begin(), qs.end());
                }
                last_rediscovery_ = now;
            }
        }

        collect_cycle();
        cycle_count_++;

        if (config_.collector.max_cycles > 0 && cycle_count_ >= config_.collector.max_cycles) {
            spdlog::info("Reached max cycles ({}), stopping", cycle_count_);
            running_ = false;
            break;
        }
    }
}

void Collector::collect_cycle() {
    try {
        metrics_collector_->collect_metrics();
        total_collections_++;
        first_collection_ = true;
    } catch (const std::exception& e) {
        spdlog::error("Collection cycle failed: {}", e.what());
        error_count_++;

        // Check for connection broken
        if (!mq_client_->is_connected()) {
            connected_qmgr_ = false;
            if (config_.collector.keep_running) {
                spdlog::warn("Connection lost during collection, triggering reconnect");
                return; // exit to reconnect loop
            }
        }
    }
}

} // namespace ibmmq_exporter
