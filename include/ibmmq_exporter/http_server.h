#pragma once

#include <memory>
#include <string>

#include <prometheus/exposer.h>
#include <prometheus/registry.h>

#include "ibmmq_exporter/config.h"

namespace ibmmq_exporter {

class HTTPServer {
public:
    HTTPServer(const PrometheusConfig& config,
               std::shared_ptr<prometheus::Registry> registry);
    ~HTTPServer();

    HTTPServer(const HTTPServer&) = delete;
    HTTPServer& operator=(const HTTPServer&) = delete;

    void start();
    void stop();

private:
    void register_landing_page();

    PrometheusConfig config_;
    std::shared_ptr<prometheus::Registry> registry_;
    std::unique_ptr<prometheus::Exposer>  exposer_;
};

} // namespace ibmmq_exporter
