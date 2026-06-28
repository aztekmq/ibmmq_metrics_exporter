#include "ibmmq_exporter/http_server.h"

#include <spdlog/spdlog.h>

namespace ibmmq_exporter {

HTTPServer::HTTPServer(const PrometheusConfig& config,
                       std::shared_ptr<prometheus::Registry> registry)
    : config_(config), registry_(std::move(registry)) {}

HTTPServer::~HTTPServer() {
    stop();
}

void HTTPServer::start() {
    auto bind_address = config_.host + ":" + std::to_string(config_.port);

    spdlog::info("Starting Prometheus HTTP server on {} path={}",
                 bind_address, config_.path);

    exposer_ = std::make_unique<prometheus::Exposer>(bind_address);
    exposer_->RegisterCollectable(registry_, config_.path);

    register_landing_page();

    spdlog::info("Prometheus metrics available at http://{}{}",
                 bind_address, config_.path);
}

void HTTPServer::stop() {
    if (exposer_) {
        spdlog::info("Stopping Prometheus HTTP server");
        exposer_.reset();
    }
}

void HTTPServer::register_landing_page() {
    if (!exposer_) return;

    const std::string metrics_path = config_.path;
    exposer_->RegisterCollectable(
        std::make_shared<prometheus::Registry>(), "/");

    // Note: prometheus-cpp's Exposer doesn't support custom HTML handlers
    // directly. The landing page is served by registering an empty registry
    // at "/" so the path is reachable. For a full HTML landing page, TLS, or
    // custom handlers, build with IBMMQ_EXPORTER_ENABLE_TLS which uses
    // cpp-httplib for full HTTP server control.
    spdlog::debug("Registered landing page handler at /");
}

} // namespace ibmmq_exporter
