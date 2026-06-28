#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "ibmmq_exporter/collector.h"
#include "ibmmq_exporter/config.h"

static std::atomic<bool> g_shutdown_requested{false};

static void signal_handler(int sig) {
    spdlog::info("Received signal {}, shutting down...", sig);
    g_shutdown_requested.store(true);
}

static void setup_logger(const std::string& level, const std::string& format,
                          bool verbose, const std::string& output_file) {
    spdlog::level::level_enum lvl = spdlog::level::info;
    if (verbose)            lvl = spdlog::level::debug;
    else if (level == "debug")   lvl = spdlog::level::debug;
    else if (level == "warn")    lvl = spdlog::level::warn;
    else if (level == "error")   lvl = spdlog::level::err;
    else if (level == "trace")   lvl = spdlog::level::trace;

    std::shared_ptr<spdlog::logger> logger;
    if (!output_file.empty()) {
        logger = spdlog::basic_logger_mt("ibmmq", output_file);
    } else {
        logger = spdlog::stderr_color_mt("ibmmq");
    }

    logger->set_level(lvl);

    if (format == "json") {
        logger->set_pattern("{\"time\":\"%Y-%m-%dT%H:%M:%S%z\",\"level\":\"%l\",\"msg\":\"%v\"}");
    } else {
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    }

    spdlog::set_default_logger(logger);
    spdlog::set_level(lvl);
}

static int run_collector(const std::string& config_file, bool continuous,
                          int interval_sec, int max_cycles, bool reset_stats,
                          int prom_port, const std::string& log_level,
                          const std::string& log_format, bool verbose) {
    setup_logger(log_level, log_format, verbose, "");

    spdlog::info("IBM MQ Statistics Collector v{}", PROJECT_VERSION);

    // Load configuration
    ibmmq_exporter::Config cfg;
    try {
        cfg = ibmmq_exporter::load_config(config_file);
    } catch (const std::exception& e) {
        spdlog::error("Failed to load configuration: {}", e.what());
        return 1;
    }

    // Override config with CLI flags
    if (continuous)    cfg.collector.continuous = true;
    if (interval_sec != 60) cfg.collector.interval = std::chrono::seconds(interval_sec);
    if (max_cycles != 0)    cfg.collector.max_cycles = max_cycles;
    if (reset_stats)        cfg.collector.reset_stats = true;
    if (prom_port != 9090)  cfg.prometheus.port = prom_port;
    if (verbose)            cfg.logging.verbose = true;
    if (log_level != "info")  cfg.logging.level = log_level;
    if (log_format != "json") cfg.logging.format = log_format;

    // Validate
    try {
        cfg.validate();
    } catch (const std::exception& e) {
        spdlog::error("Configuration validation failed: {}", e.what());
        return 1;
    }

    spdlog::info("Configuration loaded: {}", cfg.to_string());

    // Install signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Create and start collector
    ibmmq_exporter::Collector collector(cfg);

    // Run in a thread so we can check for shutdown signal
    std::thread collector_thread([&]() {
        try {
            collector.start();
        } catch (const std::exception& e) {
            spdlog::error("Collector failed: {}", e.what());
        }
    });

    // Wait for shutdown signal
    while (!g_shutdown_requested.load() && collector.is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    collector.stop();
    if (collector_thread.joinable()) collector_thread.join();

    spdlog::info("IBM MQ Statistics Collector stopped successfully");
    return 0;
}

static void print_version() {
    std::cout << "IBM MQ Statistics Collector\n"
              << "Version: " << PROJECT_VERSION << "\n"
              << "Language: C++20\n";
}

static int run_test(const std::string& config_file) {
    setup_logger("info", "text", false, "");

    spdlog::info("Testing IBM MQ connection");

    ibmmq_exporter::Config cfg;
    try {
        cfg = ibmmq_exporter::load_config(config_file);
        cfg.validate();
    } catch (const std::exception& e) {
        spdlog::error("Configuration error: {}", e.what());
        return 1;
    }

    try {
        ibmmq_exporter::MQClient client(cfg.mq);
        client.connect();
        spdlog::info("Connection successful!");
        client.disconnect();
    } catch (const std::exception& e) {
        spdlog::error("Connection test failed: {}", e.what());
        return 1;
    }

    return 0;
}

static int generate_config() {
    auto cfg = ibmmq_exporter::default_config();
    std::cout << "# IBM MQ Statistics Collector Configuration\n"
              << "# Save this as config.yaml\n\n"
              << "# IBM MQ Connection\n"
              << "mq:\n"
              << "  queue_manager: \"" << cfg.mq.queue_manager << "\"\n"
              << "  channel: \"" << cfg.mq.channel << "\"\n"
              << "  host: \"\"\n"
              << "  port: 0\n"
              << "  username: \"\"\n"
              << "  password: \"\"\n"
              << "  key_repository: \"\"\n"
              << "  cipher_spec: \"\"\n\n"
              << "# Collection Configuration\n"
              << "collector:\n"
              << "  stats_queue: \"" << cfg.collector.stats_queue << "\"\n"
              << "  accounting_queue: \"" << cfg.collector.accounting_queue << "\"\n"
              << "  reset_stats: false\n"
              << "  interval: \"" << cfg.collector.interval.count() << "s\"\n"
              << "  continuous: false\n"
              << "  monitor_all_queues: true\n"
              << "  queue_exclusion_patterns: []\n\n"
              << "  # Extended collection options\n"
              << "  keep_running: true\n"
              << "  reconnect_interval: \"" << cfg.collector.reconnect_interval.count() << "s\"\n"
              << "  use_status: true\n"
              << "  use_statistics: false\n"
              << "  use_reset_q_stats: false\n"
              << "  rediscover_interval: \"0s\"\n\n"
              << "  # Monitored object patterns (empty = monitor all)\n"
              << "  monitored_queues: []\n"
              << "  monitored_channels: []\n"
              << "  monitored_topics: []\n"
              << "  monitored_subscriptions: []\n"
              << "  monitored_amqp_channels: []\n"
              << "  monitored_mqtt_channels: []\n"
              << "  queue_subscription_selector: \"\"\n\n"
              << "# Prometheus Metrics Configuration\n"
              << "prometheus:\n"
              << "  port: " << cfg.prometheus.port << "\n"
              << "  path: \"" << cfg.prometheus.path << "\"\n"
              << "  namespace: \"" << cfg.prometheus.metrics_namespace << "\"\n"
              << "  subsystem: \"\"\n"
              << "  host: \"" << cfg.prometheus.host << "\"\n"
              << "  enable_otel: false\n"
              << "  override_c_type: false\n"
              << "  # TLS (requires build with -DIBMMQ_EXPORTER_ENABLE_TLS=ON)\n"
              << "  https_cert_file: \"\"\n"
              << "  https_key_file: \"\"\n\n"
              << "# Metadata labels added to every metric\n"
              << "metadata:\n"
              << "  meta_prefix: \"\"\n"
              << "  metadata_tags: []\n"
              << "  metadata_values: []\n"
              << "  locale: \"\"\n\n"
              << "# Logging Configuration\n"
              << "logging:\n"
              << "  level: \"" << cfg.logging.level << "\"\n"
              << "  format: \"" << cfg.logging.format << "\"\n"
              << "  output_file: \"\"\n"
              << "  verbose: false\n";
    return 0;
}

static int validate_config(const std::string& config_file) {
    setup_logger("info", "text", false, "");

    if (config_file.empty()) {
        spdlog::error("Configuration file path is required");
        return 1;
    }

    try {
        auto cfg = ibmmq_exporter::load_config(config_file);
        cfg.validate();
    } catch (const std::exception& e) {
        spdlog::error("Validation failed: {}", e.what());
        return 1;
    }

    std::cout << "Configuration file '" << config_file << "' is valid\n";
    return 0;
}

int main(int argc, char** argv) {
    CLI::App app{"IBM MQ Statistics and Accounting Collector for Prometheus"};
    app.set_version_flag("--version", std::string(PROJECT_VERSION));

    // Global flags
    std::string config_file;
    bool verbose = false;
    std::string log_level = "info";
    std::string log_format = "json";

    app.add_option("-c,--config", config_file, "Configuration file path");
    app.add_flag("-v,--verbose", verbose, "Enable verbose logging");
    app.add_option("--log-level", log_level, "Log level (debug, info, warn, error)")
       ->default_val("info");
    app.add_option("--log-format", log_format, "Log format (json, text)")
       ->default_val("json");

    // Collection flags (on main command)
    bool continuous = false;
    int interval = 60;
    int max_cycles = 0;
    bool reset_stats = false;
    int prom_port = 9090;

    app.add_flag("--continuous", continuous, "Run continuous monitoring");
    app.add_option("--interval", interval, "Collection interval in seconds")->default_val(60);
    app.add_option("--max-cycles", max_cycles, "Maximum collection cycles (0=infinite)")->default_val(0);
    app.add_flag("--reset-stats", reset_stats, "Reset statistics after reading");
    app.add_option("--prometheus-port", prom_port, "Prometheus HTTP port")->default_val(9090);

    // Subcommands
    auto* ver_cmd = app.add_subcommand("version", "Print version information");
    auto* test_cmd = app.add_subcommand("test", "Test IBM MQ connection");
    std::string test_config;
    test_cmd->add_option("-c,--config", test_config, "Configuration file path");

    auto* cfg_cmd = app.add_subcommand("config", "Configuration management");
    auto* gen_cmd = cfg_cmd->add_subcommand("generate", "Generate sample configuration");
    auto* val_cmd = cfg_cmd->add_subcommand("validate", "Validate configuration file");
    std::string val_config;
    val_cmd->add_option("-c,--config", val_config, "Configuration file path");

    app.require_subcommand(0, 1);

    CLI11_PARSE(app, argc, argv);

    if (ver_cmd->parsed()) {
        print_version();
        return 0;
    }
    if (test_cmd->parsed()) {
        return run_test(test_config.empty() ? config_file : test_config);
    }
    if (gen_cmd->parsed()) {
        return generate_config();
    }
    if (val_cmd->parsed()) {
        return validate_config(val_config.empty() ? config_file : val_config);
    }

    // Default: run collector
    return run_collector(config_file, continuous, interval, max_cycles,
                         reset_stats, prom_port, log_level, log_format, verbose);
}
