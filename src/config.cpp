#include "ibmmq_exporter/config.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

namespace ibmmq_exporter {

std::string MQConfig::get_connection_name() const {
    if (!connection_name.empty()) return connection_name;
    if (!host.empty() && port > 0)
        return host + "(" + std::to_string(port) + ")";
    return {};
}

std::string MQConfig::get_user() const {
    if (!username.empty()) return username;
    return user;
}

bool MQConfig::is_client_mode() const {
    return !get_connection_name().empty() || !host.empty();
}

const char* platform::platform_name(int32_t id) {
    switch (id) {
    case WINDOWS:   return "windows";
    case UNIX:      return "unix";
    case ZOS:       return "zos";
    case OS400:     return "os400";
    case NSK:       return "nsk";
    case APPLIANCE: return "appliance";
    default:        return "unknown";
    }
}

Config default_config() {
    Config cfg;
    cfg.mq.queue_manager   = "";
    cfg.mq.channel         = "";
    cfg.mq.connection_name = "";
    cfg.mq.host            = "";
    cfg.mq.port            = 0;

    cfg.collector.stats_queue      = "SYSTEM.ADMIN.STATISTICS.QUEUE";
    cfg.collector.accounting_queue = "SYSTEM.ADMIN.ACCOUNTING.QUEUE";
    cfg.collector.reset_stats      = false;
    cfg.collector.interval         = std::chrono::seconds(60);
    cfg.collector.max_cycles       = 0;
    cfg.collector.continuous       = false;
    cfg.collector.monitor_all_queues = true;
    cfg.collector.keep_running     = true;
    cfg.collector.reconnect_interval = std::chrono::seconds(5);
    cfg.collector.use_status       = true;
    cfg.collector.use_statistics   = false;
    cfg.collector.use_reset_q_stats = false;
    cfg.collector.use_publications  = true;
    cfg.collector.rediscover_interval = std::chrono::seconds(0);

    cfg.prometheus.port             = 9091;
    cfg.prometheus.path             = "/metrics";
    cfg.prometheus.metrics_namespace = "ibmmq";
    cfg.prometheus.enable_otel      = false;
    cfg.prometheus.host             = "0.0.0.0";
    cfg.prometheus.override_c_type  = false;

    cfg.logging.level   = "info";
    cfg.logging.format  = "json";
    cfg.logging.verbose = false;

    return cfg;
}

// Helper to read an env var
static std::string get_env(const char* name) {
    const char* val = std::getenv(name);
    return val ? std::string(val) : std::string{};
}

// Parse duration string like "10s", "30s", "1m", "5m"
static std::chrono::seconds parse_duration(const std::string& s, std::chrono::seconds def) {
    if (s.empty()) return def;
    int val = 0;
    char unit = 's';
    if (std::sscanf(s.c_str(), "%d%c", &val, &unit) >= 1) {
        if (unit == 'm') val *= 60;
        return std::chrono::seconds(val);
    }
    return def;
}

// Parse a YAML sequence into a vector of strings
static std::vector<std::string> parse_string_list(const YAML::Node& node) {
    std::vector<std::string> result;
    if (node && node.IsSequence()) {
        for (const auto& item : node)
            result.push_back(item.as<std::string>());
    }
    return result;
}

Config load_config(const std::string& config_path) {
    Config cfg = default_config();

    // Try to load YAML file
    std::string path = config_path;
    if (path.empty()) {
        // Search standard locations
        for (const auto& candidate : {"config.yaml", "configs/default.yaml",
                                       "./config/config.yaml"}) {
            std::ifstream f(candidate);
            if (f.good()) { path = candidate; break; }
        }
    }

    if (!path.empty()) {
        try {
            YAML::Node root = YAML::LoadFile(path);
            spdlog::info("Loaded configuration from {}", path);

            // MQ section
            if (auto mq = root["mq"]) {
                if (mq["queue_manager"]) cfg.mq.queue_manager   = mq["queue_manager"].as<std::string>("");
                if (mq["channel"])       cfg.mq.channel         = mq["channel"].as<std::string>("");
                if (mq["connection_name"]) cfg.mq.connection_name = mq["connection_name"].as<std::string>("");
                if (mq["host"])          cfg.mq.host            = mq["host"].as<std::string>("");
                if (mq["port"])          cfg.mq.port            = mq["port"].as<int>(0);
                if (mq["user"])          cfg.mq.user            = mq["user"].as<std::string>("");
                if (mq["username"])      cfg.mq.username        = mq["username"].as<std::string>("");
                if (mq["password"])      cfg.mq.password        = mq["password"].as<std::string>("");
                if (mq["key_repository"]) cfg.mq.key_repository = mq["key_repository"].as<std::string>("");
                if (mq["cipher_spec"])   cfg.mq.cipher_spec     = mq["cipher_spec"].as<std::string>("");
            }

            // Collector section
            if (auto col = root["collector"]) {
                if (col["stats_queue"])      cfg.collector.stats_queue      = col["stats_queue"].as<std::string>();
                if (col["accounting_queue"]) cfg.collector.accounting_queue = col["accounting_queue"].as<std::string>();
                if (col["reset_stats"])      cfg.collector.reset_stats      = col["reset_stats"].as<bool>(false);
                if (col["continuous"])        cfg.collector.continuous       = col["continuous"].as<bool>(false);
                if (col["max_cycles"])        cfg.collector.max_cycles       = col["max_cycles"].as<int>(0);
                if (col["monitor_all_queues"]) cfg.collector.monitor_all_queues = col["monitor_all_queues"].as<bool>(true);

                if (col["interval"])
                    cfg.collector.interval = parse_duration(col["interval"].as<std::string>("60s"), std::chrono::seconds(60));

                cfg.collector.queue_exclusion_patterns = parse_string_list(col["queue_exclusion_patterns"]);

                // Extended collector config
                if (col["keep_running"])      cfg.collector.keep_running = col["keep_running"].as<bool>(true);
                if (col["reconnect_interval"])
                    cfg.collector.reconnect_interval = parse_duration(col["reconnect_interval"].as<std::string>("5s"), std::chrono::seconds(5));
                if (col["use_status"])        cfg.collector.use_status = col["use_status"].as<bool>(true);
                if (col["use_statistics"])    cfg.collector.use_statistics = col["use_statistics"].as<bool>(false);
                if (col["use_reset_q_stats"]) cfg.collector.use_reset_q_stats = col["use_reset_q_stats"].as<bool>(false);
                if (col["use_publications"])  cfg.collector.use_publications = col["use_publications"].as<bool>(true);
                if (col["rediscover_interval"])
                    cfg.collector.rediscover_interval = parse_duration(col["rediscover_interval"].as<std::string>("0s"), std::chrono::seconds(0));

                cfg.collector.monitored_queues         = parse_string_list(col["monitored_queues"]);
                cfg.collector.monitored_channels       = parse_string_list(col["monitored_channels"]);
                cfg.collector.monitored_topics         = parse_string_list(col["monitored_topics"]);
                cfg.collector.monitored_subscriptions  = parse_string_list(col["monitored_subscriptions"]);
                cfg.collector.monitored_amqp_channels  = parse_string_list(col["monitored_amqp_channels"]);
                cfg.collector.monitored_mqtt_channels   = parse_string_list(col["monitored_mqtt_channels"]);
                if (col["queue_subscription_selector"])
                    cfg.collector.queue_subscription_selector = col["queue_subscription_selector"].as<std::string>("");
            }

            // Prometheus section
            if (auto prom = root["prometheus"]) {
                if (prom["port"])       cfg.prometheus.port             = prom["port"].as<int>(9091);
                if (prom["path"])       cfg.prometheus.path             = prom["path"].as<std::string>("/metrics");
                if (prom["namespace"])  cfg.prometheus.metrics_namespace = prom["namespace"].as<std::string>("ibmmq");
                if (prom["subsystem"]) cfg.prometheus.subsystem        = prom["subsystem"].as<std::string>("");
                if (prom["enable_otel"]) cfg.prometheus.enable_otel    = prom["enable_otel"].as<bool>(false);
                if (prom["host"])       cfg.prometheus.host             = prom["host"].as<std::string>("0.0.0.0");
                if (prom["https_cert_file"]) cfg.prometheus.https_cert_file = prom["https_cert_file"].as<std::string>("");
                if (prom["https_key_file"])  cfg.prometheus.https_key_file  = prom["https_key_file"].as<std::string>("");
                if (prom["override_c_type"]) cfg.prometheus.override_c_type = prom["override_c_type"].as<bool>(false);
            }

            // Metadata section
            if (auto meta = root["metadata"]) {
                if (meta["meta_prefix"])     cfg.metadata.meta_prefix     = meta["meta_prefix"].as<std::string>("");
                if (meta["locale"])          cfg.metadata.locale          = meta["locale"].as<std::string>("");
                cfg.metadata.metadata_tags   = parse_string_list(meta["metadata_tags"]);
                cfg.metadata.metadata_values = parse_string_list(meta["metadata_values"]);
            }

            // Logging section
            if (auto log = root["logging"]) {
                if (log["level"])       cfg.logging.level       = log["level"].as<std::string>("info");
                if (log["format"])      cfg.logging.format      = log["format"].as<std::string>("json");
                if (log["output_file"]) cfg.logging.output_file = log["output_file"].as<std::string>("");
                if (log["verbose"])     cfg.logging.verbose     = log["verbose"].as<bool>(false);
            }
        } catch (const YAML::Exception& e) {
            throw std::runtime_error("Error reading config file '" + path + "': " + e.what());
        }
    }

    // Build connection_name from host/port if not explicitly set
    if (cfg.mq.connection_name.empty() && !cfg.mq.host.empty() && cfg.mq.port > 0) {
        cfg.mq.connection_name = cfg.mq.host + "(" + std::to_string(cfg.mq.port) + ")";
    }

    // Override with environment variables
    auto env_val = get_env("IBMMQ_QUEUE_MANAGER");
    if (!env_val.empty()) cfg.mq.queue_manager = env_val;

    env_val = get_env("IBMMQ_CHANNEL");
    if (!env_val.empty()) cfg.mq.channel = env_val;

    env_val = get_env("IBMMQ_CONNECTION_NAME");
    if (!env_val.empty()) cfg.mq.connection_name = env_val;

    env_val = get_env("IBMMQ_USER");
    if (!env_val.empty()) cfg.mq.user = env_val;

    env_val = get_env("IBMMQ_PASSWORD");
    if (!env_val.empty()) cfg.mq.password = env_val;

    env_val = get_env("IBMMQ_KEY_REPOSITORY");
    if (!env_val.empty()) cfg.mq.key_repository = env_val;

    env_val = get_env("IBMMQ_CIPHER_SPEC");
    if (!env_val.empty()) cfg.mq.cipher_spec = env_val;

    // Force override_c_type when use_statistics is enabled
    if (cfg.collector.use_statistics) {
        cfg.prometheus.override_c_type = true;
    }

    return cfg;
}

void Config::validate() const {
    if (mq.queue_manager.empty())
        throw std::runtime_error("queue manager name is required");
    // Channel and connection are only required in client mode
    if (mq.is_client_mode()) {
        if (mq.channel.empty())
            throw std::runtime_error("channel name is required for client mode");
        if (mq.get_connection_name().empty())
            throw std::runtime_error("connection name is required (provide either connection_name or host/port)");
    }
    if (collector.interval < std::chrono::seconds(1))
        throw std::runtime_error("collection interval must be at least 1 second");
    if (prometheus.port < 1 || prometheus.port > 65535)
        throw std::runtime_error("prometheus port must be between 1 and 65535");
    if (metadata.metadata_tags.size() != metadata.metadata_values.size() && !metadata.metadata_tags.empty())
        throw std::runtime_error("metadata_tags and metadata_values must have the same length");
}

std::string Config::to_string() const {
    std::ostringstream oss;
    oss << "QM: " << mq.queue_manager
        << ", Channel: " << mq.channel
        << ", Connection: " << mq.get_connection_name()
        << ", User: " << mq.get_user()
        << ", Mode: " << (mq.is_client_mode() ? "client" : "binding")
        << ", StatsQueue: " << collector.stats_queue
        << ", AccountingQueue: " << collector.accounting_queue
        << ", UseStatus: " << (collector.use_status ? "true" : "false")
        << ", UseStatistics: " << (collector.use_statistics ? "true" : "false")
        << ", UsePublications: " << (collector.use_publications ? "true" : "false")
        << ", KeepRunning: " << (collector.keep_running ? "true" : "false");
    return oss.str();
}

} // namespace ibmmq_exporter
