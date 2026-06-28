#pragma once

#include <chrono>
#include <map>
#include <string>
#include <vector>

namespace ibmmq_exporter {

struct MQConfig {
    std::string queue_manager;
    std::string channel;
    std::string connection_name;
    std::string host;
    int         port{0};
    std::string user;
    std::string username;
    std::string password;
    std::string key_repository;
    std::string cipher_spec;

    [[nodiscard]] std::string get_connection_name() const;
    [[nodiscard]] std::string get_user() const;
    [[nodiscard]] bool is_client_mode() const;
};

struct CollectorConfig {
    std::string                stats_queue{"SYSTEM.ADMIN.STATISTICS.QUEUE"};
    std::string                accounting_queue{"SYSTEM.ADMIN.ACCOUNTING.QUEUE"};
    bool                       reset_stats{false};
    std::chrono::seconds       interval{60};
    int                        max_cycles{0};
    bool                       continuous{false};
    bool                       monitor_all_queues{true};
    std::vector<std::string>   queue_exclusion_patterns;

    // Extended config (matching Go mq_prometheus features)
    bool                       keep_running{true};
    std::chrono::seconds       reconnect_interval{5};
    bool                       use_status{true};
    bool                       use_statistics{false};
    bool                       use_reset_q_stats{false};
    bool                       use_publications{true};  // Enable $SYS topic subscription metrics (amqsrua-style)
    std::chrono::seconds       rediscover_interval{0};

    // Monitored object patterns
    std::vector<std::string>   monitored_queues;
    std::vector<std::string>   monitored_channels;
    std::vector<std::string>   monitored_topics;
    std::vector<std::string>   monitored_subscriptions;
    std::vector<std::string>   monitored_amqp_channels;
    std::vector<std::string>   monitored_mqtt_channels;
    std::string                queue_subscription_selector;
};

struct PrometheusConfig {
    int         port{9091};
    std::string path{"/metrics"};
    std::string metrics_namespace{"ibmmq"};
    std::string subsystem;
    bool        enable_otel{false};

    // Extended config
    std::string host{"0.0.0.0"};
    std::string https_cert_file;
    std::string https_key_file;
    bool        override_c_type{false};
};

struct MetadataConfig {
    std::string              meta_prefix;
    std::vector<std::string> metadata_tags;
    std::vector<std::string> metadata_values;
    std::string              locale;
};

struct LoggingConfig {
    std::string level{"info"};
    std::string format{"json"};
    std::string output_file;
    bool        verbose{false};
};

struct Config {
    MQConfig         mq;
    CollectorConfig  collector;
    PrometheusConfig prometheus;
    MetadataConfig   metadata;
    LoggingConfig    logging;

    [[nodiscard]] std::string to_string() const;
    void validate() const; // throws std::runtime_error on failure
};

// Platform constants (from MQIA_PLATFORM inquiry)
namespace platform {
    constexpr int32_t UNKNOWN    = 0;
    constexpr int32_t OS400     = 3;
    constexpr int32_t WINDOWS   = 11;
    constexpr int32_t UNIX      = 13;
    constexpr int32_t ZOS       = 18;
    constexpr int32_t NSK       = 27;
    constexpr int32_t APPLIANCE = 28;

    // Compile-time detection of local platform
    constexpr int32_t local_platform() {
#ifdef _WIN32
        return WINDOWS;
#elif defined(__MVS__)
        return ZOS;
#elif defined(__OS400__)
        return OS400;
#else
        return UNIX;
#endif
    }

    const char* platform_name(int32_t id);
} // namespace platform

// Load configuration from YAML file, environment variables, and defaults
Config load_config(const std::string& config_path);

// Return a default configuration
Config default_config();

} // namespace ibmmq_exporter
