#include "ibmmq_exporter/metrics_collector.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <spdlog/spdlog.h>

namespace ibmmq_exporter {

MetricsCollector::MetricsCollector(const Config& config,
                                   MQClient& mq_client,
                                   std::shared_ptr<prometheus::Registry> registry)
    : config_(config), mq_client_(&mq_client), registry_(std::move(registry)) {
    init_metrics();
}

std::map<std::string, std::string> MetricsCollector::add_meta_labels(
        std::map<std::string, std::string> labels) const {
    const auto& tags = config_.metadata.metadata_tags;
    const auto& vals = config_.metadata.metadata_values;
    for (size_t i = 0; i < tags.size() && i < vals.size(); ++i) {
        labels[tags[i]] = vals[i];
    }
    return labels;
}

void MetricsCollector::init_metrics() {
    const auto& ns = config_.prometheus.metrics_namespace;
    const auto& sub = config_.prometheus.subsystem;
    auto prefix = ns + (sub.empty() ? "" : "_" + sub);

    // Queue metrics
    queue_depth_ = &prometheus::BuildGauge().Name(prefix + "_queue_depth")
        .Help("Current depth of IBM MQ queue").Register(*registry_);
    queue_high_depth_ = &prometheus::BuildGauge().Name(prefix + "_queue_depth_high")
        .Help("High water mark of IBM MQ queue depth").Register(*registry_);
    queue_enqueue_ = &prometheus::BuildGauge().Name(prefix + "_queue_enqueue_count")
        .Help("Total messages enqueued to IBM MQ queue").Register(*registry_);
    queue_dequeue_ = &prometheus::BuildGauge().Name(prefix + "_queue_dequeue_count")
        .Help("Total messages dequeued from IBM MQ queue").Register(*registry_);
    queue_input_handles_ = &prometheus::BuildGauge().Name(prefix + "_queue_input_handles")
        .Help("Input handles open on a queue").Register(*registry_);
    queue_output_handles_ = &prometheus::BuildGauge().Name(prefix + "_queue_output_handles")
        .Help("Output handles open on a queue").Register(*registry_);
    queue_readers_ = &prometheus::BuildGauge().Name(prefix + "_queue_has_readers")
        .Help("Whether IBM MQ queue has active readers (1=yes, 0=no)").Register(*registry_);
    queue_writers_ = &prometheus::BuildGauge().Name(prefix + "_queue_has_writers")
        .Help("Whether IBM MQ queue has active writers (1=yes, 0=no)").Register(*registry_);
    queue_process_ = &prometheus::BuildGauge().Name(prefix + "_queue_process")
        .Help("Processes associated with a queue").Register(*registry_);
    queue_max_depth_ = &prometheus::BuildGauge().Name(prefix + "_queue_attribute_max_depth")
        .Help("Maximum queue depth attribute").Register(*registry_);
    queue_oldest_msg_age_ = &prometheus::BuildGauge().Name(prefix + "_queue_oldest_message_age")
        .Help("Age of oldest message on queue in seconds").Register(*registry_);
    queue_uncommitted_msgs_ = &prometheus::BuildGauge().Name(prefix + "_queue_uncommitted_messages")
        .Help("Number of uncommitted messages on queue").Register(*registry_);
    queue_qtime_short_ = &prometheus::BuildGauge().Name(prefix + "_queue_qtime_short")
        .Help("Queue time indicator (short sample) microseconds").Register(*registry_);
    queue_qtime_long_ = &prometheus::BuildGauge().Name(prefix + "_queue_qtime_long")
        .Help("Queue time indicator (long sample) microseconds").Register(*registry_);
    queue_qfile_current_size_ = &prometheus::BuildGauge().Name(prefix + "_queue_qfile_current_size")
        .Help("Current queue file size in bytes").Register(*registry_);
    queue_qfile_max_size_ = &prometheus::BuildGauge().Name(prefix + "_queue_qfile_max_size")
        .Help("Maximum queue file size in bytes").Register(*registry_);

    // Per-queue per-app metrics
    queue_app_puts_ = &prometheus::BuildGauge().Name(prefix + "_queue_app_puts_total")
        .Help("PUTs on a queue by a specific application").Register(*registry_);
    queue_app_gets_ = &prometheus::BuildGauge().Name(prefix + "_queue_app_gets_total")
        .Help("GETs on a queue by a specific application").Register(*registry_);
    queue_app_msgs_received_ = &prometheus::BuildGauge().Name(prefix + "_queue_app_msgs_received_total")
        .Help("Messages received on a queue by a specific application").Register(*registry_);
    queue_app_msgs_sent_ = &prometheus::BuildGauge().Name(prefix + "_queue_app_msgs_sent_total")
        .Help("Messages sent on a queue by a specific application").Register(*registry_);

    // Handle metrics
    queue_handle_count_ = &prometheus::BuildGauge().Name(prefix + "_queue_handle_count")
        .Help("Total open handles on a queue").Register(*registry_);
    queue_handle_info_ = &prometheus::BuildGauge().Name(prefix + "_queue_handle_info")
        .Help("Information about open handles on a queue").Register(*registry_);
    queue_process_detail_ = &prometheus::BuildGauge().Name(prefix + "_queue_process_detail")
        .Help("Process details accessing a queue").Register(*registry_);

    // Channel metrics (statistics)
    channel_messages_ = &prometheus::BuildGauge().Name(prefix + "_channel_messages_total")
        .Help("Messages sent through IBM MQ channel").Register(*registry_);
    channel_bytes_ = &prometheus::BuildGauge().Name(prefix + "_channel_bytes_total")
        .Help("Bytes sent through IBM MQ channel").Register(*registry_);
    channel_batches_ = &prometheus::BuildGauge().Name(prefix + "_channel_batches_total")
        .Help("Batches sent through IBM MQ channel").Register(*registry_);

    // Channel status metrics (PCF inquiry)
    channel_status_ = &prometheus::BuildGauge().Name(prefix + "_channel_status")
        .Help("Channel status from PCF inquiry").Register(*registry_);
    channel_status_msgs_ = &prometheus::BuildGauge().Name(prefix + "_channel_msgs")
        .Help("Messages through channel from PCF status").Register(*registry_);
    channel_bytes_sent_ = &prometheus::BuildGauge().Name(prefix + "_channel_bytes_sent")
        .Help("Bytes sent through channel from PCF status").Register(*registry_);
    channel_bytes_received_ = &prometheus::BuildGauge().Name(prefix + "_channel_bytes_received")
        .Help("Bytes received through channel from PCF status").Register(*registry_);
    channel_buffers_sent_ = &prometheus::BuildGauge().Name(prefix + "_channel_buffers_sent")
        .Help("Buffers sent through channel").Register(*registry_);
    channel_buffers_received_ = &prometheus::BuildGauge().Name(prefix + "_channel_buffers_received")
        .Help("Buffers received through channel").Register(*registry_);
    channel_substate_ = &prometheus::BuildGauge().Name(prefix + "_channel_substate")
        .Help("Channel substate").Register(*registry_);
    channel_instance_type_ = &prometheus::BuildGauge().Name(prefix + "_channel_instance_type")
        .Help("Channel instance type").Register(*registry_);
    channel_nettime_short_ = &prometheus::BuildGauge().Name(prefix + "_channel_nettime_short")
        .Help("Network time indicator (short sample) microseconds").Register(*registry_);
    channel_nettime_long_ = &prometheus::BuildGauge().Name(prefix + "_channel_nettime_long")
        .Help("Network time indicator (long sample) microseconds").Register(*registry_);
    channel_xmitq_time_short_ = &prometheus::BuildGauge().Name(prefix + "_channel_xmitq_time_short")
        .Help("Transmission queue time indicator (short sample) microseconds").Register(*registry_);
    channel_xmitq_time_long_ = &prometheus::BuildGauge().Name(prefix + "_channel_xmitq_time_long")
        .Help("Transmission queue time indicator (long sample) microseconds").Register(*registry_);
    channel_batch_size_short_ = &prometheus::BuildGauge().Name(prefix + "_channel_batch_size_short")
        .Help("Batch size indicator (short sample)").Register(*registry_);
    channel_batch_size_long_ = &prometheus::BuildGauge().Name(prefix + "_channel_batch_size_long")
        .Help("Batch size indicator (long sample)").Register(*registry_);
    channel_max_instc_ = &prometheus::BuildGauge().Name(prefix + "_channel_max_instc")
        .Help("Maximum instances for channel").Register(*registry_);
    channel_cur_instc_ = &prometheus::BuildGauge().Name(prefix + "_channel_cur_instc")
        .Help("Current sharing conversations for channel").Register(*registry_);
    channel_status_squash_ = &prometheus::BuildGauge().Name(prefix + "_channel_status_squash")
        .Help("Simplified channel status (0=OK, 1=transitioning, 2=stopped, 3=unknown)").Register(*registry_);

    // Topic status metrics
    topic_pub_count_ = &prometheus::BuildGauge().Name(prefix + "_topic_publisher_count")
        .Help("Number of publishers on topic").Register(*registry_);
    topic_sub_count_ = &prometheus::BuildGauge().Name(prefix + "_topic_subscriber_count")
        .Help("Number of subscribers on topic").Register(*registry_);

    // Subscription status metrics
    sub_durable_ = &prometheus::BuildGauge().Name(prefix + "_subscription_durable")
        .Help("Whether subscription is durable").Register(*registry_);
    sub_type_ = &prometheus::BuildGauge().Name(prefix + "_subscription_type")
        .Help("Subscription type").Register(*registry_);
    sub_message_count_ = &prometheus::BuildGauge().Name(prefix + "_subscription_messages_received")
        .Help("Messages received by subscription").Register(*registry_);

    // QM status metrics
    qmgr_status_ = &prometheus::BuildGauge().Name(prefix + "_qmgr_status")
        .Help("Queue manager status").Register(*registry_);
    qmgr_connection_count_ = &prometheus::BuildGauge().Name(prefix + "_qmgr_connection_count")
        .Help("Queue manager connection count").Register(*registry_);
    qmgr_chinit_status_ = &prometheus::BuildGauge().Name(prefix + "_qmgr_channel_initiator_status")
        .Help("Channel initiator status").Register(*registry_);
    qmgr_cmd_server_status_ = &prometheus::BuildGauge().Name(prefix + "_qmgr_command_server_status")
        .Help("Command server status").Register(*registry_);
    qmgr_uptime_ = &prometheus::BuildGauge().Name(prefix + "_qmgr_uptime")
        .Help("Queue manager uptime in seconds").Register(*registry_);

    // Cluster metrics
    cluster_qmgr_status_ = &prometheus::BuildGauge().Name(prefix + "_cluster_qmgr_status")
        .Help("Cluster queue manager status").Register(*registry_);
    cluster_qmgr_suspend_ = &prometheus::BuildGauge().Name(prefix + "_cluster_qmgr_suspend")
        .Help("Whether queue manager is suspended from cluster").Register(*registry_);

    // z/OS Buffer Pool metrics
    usage_bp_free_ = &prometheus::BuildGauge().Name(prefix + "_usage_bp_free_buffers")
        .Help("Free buffers in buffer pool (z/OS)").Register(*registry_);
    usage_bp_total_ = &prometheus::BuildGauge().Name(prefix + "_usage_bp_total_buffers")
        .Help("Total buffers in buffer pool (z/OS)").Register(*registry_);

    // z/OS Page Set metrics
    usage_ps_total_ = &prometheus::BuildGauge().Name(prefix + "_usage_ps_total_pages")
        .Help("Total pages in page set (z/OS)").Register(*registry_);
    usage_ps_unused_ = &prometheus::BuildGauge().Name(prefix + "_usage_ps_unused_pages")
        .Help("Unused pages in page set (z/OS)").Register(*registry_);

    // MQI metrics map
    struct MetricDef { std::string name; std::string help; };
    std::vector<MetricDef> mqi_defs = {
        {"opens", "MQI OPEN operations"}, {"closes", "MQI CLOSE operations"},
        {"puts", "MQI PUT operations"}, {"gets", "MQI GET operations"},
        {"commits", "MQI COMMIT operations"}, {"backouts", "MQI BACKOUT operations"},
        {"browses", "MQI BROWSE operations"}, {"inqs", "MQI INQUIRE operations"},
        {"sets", "MQI SET operations"},
        {"disc_close_timeout", "Disconnections due to close timeout"},
        {"disc_reset_timeout", "Disconnections due to reset timeout"},
        {"fails", "MQI failures"}, {"incomplete_batch", "Incomplete batches"},
        {"incomplete_msg", "Incomplete messages"}, {"wait_interval", "Wait interval"},
        {"syncpoint_heuristic", "Syncpoint heuristic decisions"},
        {"heaps", "Heaps allocated"}, {"logical_connections", "Logical connections"},
        {"physical_connections", "Physical connections"}, {"current_conns", "Current connections"},
        {"persistent_msgs", "Persistent messages"}, {"non_persistent_msgs", "Non-persistent messages"},
        {"long_msgs", "Long messages"}, {"short_msgs", "Short messages"},
        {"stamp_enabled", "Timestamp enabled"}, {"msgs_received", "Messages received"},
        {"msgs_sent", "Messages sent"}, {"channel_status", "Channel status"},
        {"channel_type", "Channel type"}, {"channel_errors", "Channel errors"},
        {"channel_disc_count", "Channel disconnections"},
        {"full_batches", "Full batches"}, {"partial_batches", "Partial batches"},
        {"queue_time", "Queue time ms"}, {"queue_time_max", "Max queue time ms"},
        {"elapsed_time", "Elapsed time ms"}, {"elapsed_time_max", "Max elapsed time ms"},
        {"conn_time", "Connection time ms"}, {"conn_time_max", "Max connection time ms"},
        {"bytes_received", "Bytes received"}, {"bytes_sent", "Bytes sent"},
        {"backout_count", "Backouts"}, {"commits_count", "Commits"},
        {"rollback_count", "Rollbacks"},
    };

    for (const auto& d : mqi_defs) {
        mqi_metrics_[d.name] = &prometheus::BuildGauge()
            .Name(prefix + "_mqi_" + d.name).Help(d.help).Register(*registry_);
    }

    // Collection info
    collection_info_ = &prometheus::BuildGauge().Name(prefix + "_collection_info")
        .Help("Collection process information").Register(*registry_);
    last_collection_time_ = &prometheus::BuildGauge().Name(prefix + "_last_collection_timestamp")
        .Help("Timestamp of last successful collection").Register(*registry_);
    publications_count_ = &prometheus::BuildGauge().Name(prefix + "_publications_received")
        .Help("Number of publications received").Register(*registry_);
}

void MetricsCollector::collect_metrics() {
    std::lock_guard lock(mu_);

    spdlog::info("Starting metrics collection");

    auto stats_msgs = collect_messages("stats");
    auto acct_msgs  = collect_messages("accounting");

    update_metrics_from_messages(stats_msgs, acct_msgs);

    // Cache queue list once per cycle to avoid redundant PCF discovery
    cached_queues_ = get_queues_to_monitor();

    try { collect_and_update_queue_metrics(); }
    catch (const std::exception& e) { spdlog::error("Queue metrics failed: {}", e.what()); }

    try { collect_and_update_handle_metrics(); }
    catch (const std::exception& e) { spdlog::error("Handle metrics failed: {}", e.what()); }

    // Extended PCF status collection — each wrapped so one failure doesn't block others
    if (config_.collector.use_status && mq_client_->is_connected()) {
        try { collect_qmgr_status(); }
        catch (const std::exception& e) { spdlog::error("QMgr status failed: {}", e.what()); }

        try { collect_channel_status(); }
        catch (const std::exception& e) { spdlog::error("Channel status failed: {}", e.what()); }

        try { collect_topic_status(); }
        catch (const std::exception& e) { spdlog::error("Topic status failed: {}", e.what()); }

        try { collect_sub_status(); }
        catch (const std::exception& e) { spdlog::error("Sub status failed: {}", e.what()); }

        try { collect_cluster_status(); }
        catch (const std::exception& e) { spdlog::error("Cluster status failed: {}", e.what()); }

        try { collect_queue_online_status(); }
        catch (const std::exception& e) { spdlog::error("Queue online status failed: {}", e.what()); }

        // z/OS usage only on z/OS platform
        if (mq_client_->get_platform() == platform::ZOS) {
            try { collect_usage_status(); }
            catch (const std::exception& e) { spdlog::error("Usage status failed: {}", e.what()); }
        }
    }

    // Publication-based metrics from $SYS topics
    if (resource_monitor_ && mq_client_->is_connected()) {
        try { collect_publication_metrics(); }
        catch (const std::exception& e) { spdlog::error("Publication metrics failed: {}", e.what()); }
    }

    cached_queues_.clear();

    set_baseline_metrics(static_cast<int>(stats_msgs.size()),
                         static_cast<int>(acct_msgs.size()));

    spdlog::info("Completed metrics collection: stats={}, accounting={}",
                 stats_msgs.size(), acct_msgs.size());
}

std::vector<MQMessage> MetricsCollector::collect_messages(const std::string& queue_type) {
    try {
        return mq_client_->get_all_messages(queue_type);
    } catch (const std::exception& e) {
        spdlog::error("Failed to collect {} messages: {}", queue_type, e.what());
        return {};
    }
}

void MetricsCollector::update_metrics_from_messages(
        const std::vector<MQMessage>& stats_msgs,
        const std::vector<MQMessage>& acct_msgs) {
    for (const auto& msg : stats_msgs) process_statistics_message(msg);
    for (const auto& msg : acct_msgs)  process_accounting_message(msg);

    collection_info_->Add(add_meta_labels({
        {"queue_manager", config_.mq.queue_manager},
        {"channel", config_.mq.channel},
        {"platform", mq_client_->get_platform_string()},
        {"collector_version", "1.0.0"}})).Set(1);
}

void MetricsCollector::update_mqi_gauges(const MQIStatistics& mqi, const std::string& qmgr) {
    auto labels = add_meta_labels({
        {"queue_manager", qmgr},
        {"application_name", mqi.application_name},
        {"application_tag", mqi.application_tag},
        {"user_identifier", mqi.user_identifier},
        {"connection_name", mqi.connection_name},
        {"channel_name", mqi.channel_name},
    });

    auto set = [&](const std::string& name, double val) {
        if (auto it = mqi_metrics_.find(name); it != mqi_metrics_.end())
            it->second->Add(labels).Set(val);
    };

    set("opens", mqi.opens);     set("closes", mqi.closes);
    set("puts", mqi.puts);       set("gets", mqi.gets);
    set("commits", mqi.commits); set("backouts", mqi.backouts);
    set("browses", mqi.browses); set("inqs", mqi.inqs);
    set("sets", mqi.sets);
    set("disc_close_timeout", mqi.disc_close_timeout);
    set("disc_reset_timeout", mqi.disc_reset_timeout);
    set("fails", mqi.fails);
    set("incomplete_batch", mqi.incomplete_batch);
    set("incomplete_msg", mqi.incomplete_msg);
    set("wait_interval", mqi.wait_interval);
    set("syncpoint_heuristic", mqi.syncpoint_heuristic);
    set("heaps", mqi.heaps);
    set("logical_connections", mqi.logical_connections);
    set("physical_connections", mqi.physical_connections);
    set("current_conns", mqi.current_conns);
    set("persistent_msgs", mqi.persistent_msgs);
    set("non_persistent_msgs", mqi.non_persistent_msgs);
    set("long_msgs", mqi.long_msgs);
    set("short_msgs", mqi.short_msgs);
    set("stamp_enabled", mqi.stamp_enabled);
    set("msgs_received", mqi.msgs_received);
    set("msgs_sent", mqi.msgs_sent);
    set("channel_status", mqi.channel_status);
    set("channel_type", mqi.channel_type);
    set("channel_errors", mqi.channel_errors);
    set("channel_disc_count", mqi.channel_disc_count);
    set("full_batches", mqi.full_batches);
    set("partial_batches", mqi.partial_batches);
    set("queue_time", static_cast<double>(mqi.queue_time));
    set("queue_time_max", static_cast<double>(mqi.queue_time_max));
    set("elapsed_time", static_cast<double>(mqi.elapsed_time));
    set("elapsed_time_max", static_cast<double>(mqi.elapsed_time_max));
    set("conn_time", static_cast<double>(mqi.conn_time));
    set("conn_time_max", static_cast<double>(mqi.conn_time_max));
    set("bytes_received", static_cast<double>(mqi.bytes_received));
    set("bytes_sent", static_cast<double>(mqi.bytes_sent));
    set("backout_count", static_cast<double>(mqi.backout_count));
    set("commits_count", static_cast<double>(mqi.commits_count));
    set("rollback_count", static_cast<double>(mqi.rollback_count));
}

void MetricsCollector::process_statistics_message(const MQMessage& msg) {
    auto result = pcf_parser_.parse_message(msg.data, "statistics");
    if (!result) return;

    auto* stats = std::get_if<StatisticsData>(&*result);
    if (!stats) return;

    auto qmgr = stats->queue_manager.empty() ? config_.mq.queue_manager : stats->queue_manager;
    auto plat = mq_client_->get_platform_string();

    if (stats->queue_stats) {
        auto& qs = *stats->queue_stats;
        auto ql = add_meta_labels({{"queue_manager", qmgr}, {"queue_name", qs.queue_name}, {"platform", plat}});

        queue_depth_->Add(ql).Set(qs.current_depth);
        queue_high_depth_->Add(ql).Set(qs.high_depth);
        queue_enqueue_->Add(ql).Set(qs.enqueue_count);
        queue_dequeue_->Add(ql).Set(qs.dequeue_count);
        queue_readers_->Add(ql).Set(qs.has_readers ? 1.0 : 0.0);
        queue_writers_->Add(ql).Set(qs.has_writers ? 1.0 : 0.0);

        for (const auto& proc : qs.associated_procs) {
            queue_process_->Add(add_meta_labels({{"queue_manager", qmgr},
                                 {"queue_name", qs.queue_name},
                                 {"application_name", proc.application_name},
                                 {"source_ip", proc.connection_name},
                                 {"role", proc.role}})).Set(1);

            queue_handle_info_->Add(add_meta_labels({{"queue_manager", qmgr},
                                     {"queue_name", qs.queue_name},
                                     {"application_name", proc.application_name},
                                     {"user_identifier", proc.user_identifier},
                                     {"open_mode", proc.role == "input" ? "Input" : (proc.role == "output" ? "Output" : "Unknown")},
                                     {"handle_state", "Open"}})).Set(1);

            queue_process_detail_->Add(add_meta_labels({{"queue_manager", qmgr},
                                        {"queue_name", qs.queue_name},
                                        {"application_name", proc.application_name},
                                        {"process_id", std::to_string(proc.process_id)},
                                        {"user_identifier", proc.user_identifier},
                                        {"role", proc.role}})).Set(1);

            auto* target = (proc.role == "input") ? queue_input_handles_ : queue_output_handles_;
            if (target) {
                target->Add(add_meta_labels({{"queue_manager", qmgr},
                             {"queue_name", qs.queue_name},
                             {"userid", proc.user_identifier},
                             {"pid", std::to_string(proc.process_id)},
                             {"channel", proc.channel_name},
                             {"appltag", proc.application_tag},
                             {"conname", proc.connection_name}})).Set(1);
            }
        }
    }

    if (stats->channel_stats) {
        auto& cs = *stats->channel_stats;
        auto cl = add_meta_labels({{"queue_manager", qmgr},
            {"channel_name", cs.channel_name},
            {"connection_name", cs.connection_name}});
        channel_messages_->Add(cl).Set(cs.messages);
        channel_bytes_->Add(cl).Set(static_cast<double>(cs.bytes));
        channel_batches_->Add(cl).Set(cs.batches);
    }

    if (stats->mqi_stats) {
        update_mqi_gauges(*stats->mqi_stats, qmgr);
    }
}

void MetricsCollector::process_accounting_message(const MQMessage& msg) {
    auto result = pcf_parser_.parse_message(msg.data, "accounting");
    if (!result) return;

    auto* acct = std::get_if<AccountingData>(&*result);
    if (!acct) return;

    auto qmgr = acct->queue_manager.empty() ? config_.mq.queue_manager : acct->queue_manager;

    if (acct->operations) {
        std::string app, tag, uid, conn, chan;
        if (acct->connection_info) {
            app  = acct->connection_info->application_name;
            tag  = acct->connection_info->application_tag;
            uid  = acct->connection_info->user_identifier;
            conn = acct->connection_info->connection_name;
            chan  = acct->connection_info->channel_name;
        }
        auto labels = add_meta_labels({
            {"queue_manager", qmgr},
            {"application_name", app}, {"application_tag", tag},
            {"user_identifier", uid}, {"connection_name", conn},
            {"channel_name", chan}});

        auto add = [&](const std::string& name, double v) {
            if (auto it = mqi_metrics_.find(name); it != mqi_metrics_.end())
                it->second->Add(labels).Increment(v);
        };
        auto& ops = *acct->operations;
        add("opens", ops.opens); add("closes", ops.closes);
        add("puts", ops.puts);   add("gets", ops.gets);
        add("commits", ops.commits); add("backouts", ops.backouts);
    }

    for (const auto& qa : acct->queue_operations) {
        auto ql = add_meta_labels({
            {"queue_manager", qmgr}, {"queue_name", qa.queue_name},
            {"application_name", qa.application_name},
            {"source_ip", qa.connection_name},
            {"user_identifier", qa.user_identifier}});

        queue_app_puts_->Add(ql).Set(qa.puts);
        queue_app_gets_->Add(ql).Set(qa.gets);
        queue_app_msgs_received_->Add(ql).Set(qa.msgs_received);
        queue_app_msgs_sent_->Add(ql).Set(qa.msgs_sent);
    }
}

void MetricsCollector::collect_and_update_queue_metrics() {
    if (!mq_client_->is_connected()) return;

    auto plat = mq_client_->get_platform_string();

    for (const auto& qname : cached_queues_) {
        auto info = mq_client_->get_queue_info(qname);
        if (!info) continue;

        auto labels = add_meta_labels({{"queue_manager", config_.mq.queue_manager},
                                       {"queue_name", info->queue_name},
                                       {"platform", plat}});

        queue_depth_->Add(labels).Set(info->current_depth);
        queue_max_depth_->Add(labels).Set(info->max_queue_depth);
    }
}

std::vector<std::string> MetricsCollector::get_queues_to_monitor() {
    if (!config_.collector.monitor_all_queues && config_.collector.monitored_queues.empty())
        return {};

    // If we have discovered queues via PCF, use those
    if (!config_.collector.monitored_queues.empty()) {
        // Try PCF discovery first
        std::vector<std::string> all_queues;
        for (const auto& pattern : config_.collector.monitored_queues) {
            auto qs = mq_client_->discover_queues(pattern);
            all_queues.insert(all_queues.end(), qs.begin(), qs.end());
        }
        if (!all_queues.empty()) {
            std::vector<std::string> result;
            for (const auto& q : all_queues) {
                if (!queue_matches_exclusion(q, config_.collector.queue_exclusion_patterns))
                    result.push_back(q);
            }
            return result;
        }
    }

    // Fallback: try wildcard discovery
    auto all_queues = mq_client_->discover_queues("*");
    if (!all_queues.empty()) {
        std::vector<std::string> result;
        for (const auto& q : all_queues) {
            if (!queue_matches_exclusion(q, config_.collector.queue_exclusion_patterns))
                result.push_back(q);
        }
        return result;
    }

    // Ultimate fallback: system queues
    return {"SYSTEM.ADMIN.STATISTICS.QUEUE", "SYSTEM.ADMIN.ACCOUNTING.QUEUE"};
}

void MetricsCollector::collect_and_update_handle_metrics() {
    if (!mq_client_->is_connected()) return;
    if (cached_queues_.empty()) return;

    for (const auto& qname : cached_queues_) {
        auto info = mq_client_->get_queue_info(qname);
        if (!info) continue;

        if (info->open_input_count > 0) {
            queue_input_handles_->Add(add_meta_labels({
                {"queue_manager", config_.mq.queue_manager},
                {"queue_name", qname},
                {"userid", ""}, {"pid", ""}, {"channel", ""},
                {"appltag", ""}, {"conname", ""}
            })).Set(info->open_input_count);
        }
        if (info->open_output_count > 0) {
            queue_output_handles_->Add(add_meta_labels({
                {"queue_manager", config_.mq.queue_manager},
                {"queue_name", qname},
                {"userid", ""}, {"pid", ""}, {"channel", ""},
                {"appltag", ""}, {"conname", ""}
            })).Set(info->open_output_count);
        }
    }
}

// --- Extended PCF status collection ---

void MetricsCollector::collect_queue_online_status() {
    if (!mq_client_->is_connected()) return;
    if (cached_queues_.empty()) return;

    auto plat = mq_client_->get_platform_string();

    // Use wildcard query to get all queue statuses in one PCF command
    auto statuses = mq_client_->get_queue_online_status("*");

    // Build a set of monitored queue names for fast lookup
    std::set<std::string> monitored(cached_queues_.begin(), cached_queues_.end());

    for (const auto& qs : statuses) {
        if (monitored.find(qs.queue_name) == monitored.end()) continue;

        auto labels = add_meta_labels({{"queue_manager", config_.mq.queue_manager},
                                       {"queue_name", qs.queue_name},
                                       {"platform", plat}});

        queue_oldest_msg_age_->Add(labels).Set(qs.oldest_msg_age);
        queue_uncommitted_msgs_->Add(labels).Set(qs.uncommitted_msgs);
        queue_qtime_short_->Add(labels).Set(qs.qtime_short);
        queue_qtime_long_->Add(labels).Set(qs.qtime_long);
        queue_qfile_current_size_->Add(labels).Set(qs.cur_q_file_size);
        queue_qfile_max_size_->Add(labels).Set(qs.cur_max_file_size);
    }
}

void MetricsCollector::collect_channel_status() {
    auto patterns = config_.collector.monitored_channels;
    if (patterns.empty()) patterns.push_back("*");

    auto plat = mq_client_->get_platform_string();

    for (const auto& pattern : patterns) {
        auto channels = mq_client_->get_channel_status(pattern);
        for (const auto& ch : channels) {
            auto labels = add_meta_labels({
                {"qmgr", config_.mq.queue_manager},
                {"platform", plat},
                {"channel", ch.channel_name},
                {"type", std::to_string(ch.channel_type)},
                {"rqmname", ch.remote_qmgr},
                {"connname", ch.connection_name},
                {"jobname", ch.job_name},
                {"sslciph", ch.ssl_cipher}});

            channel_status_->Add(labels).Set(ch.status);
            channel_status_msgs_->Add(labels).Set(ch.msgs);
            channel_bytes_sent_->Add(labels).Set(static_cast<double>(ch.bytes_sent));
            channel_bytes_received_->Add(labels).Set(static_cast<double>(ch.bytes_received));
            channel_buffers_sent_->Add(labels).Set(ch.buffers_sent);
            channel_buffers_received_->Add(labels).Set(ch.buffers_received);
            channel_substate_->Add(labels).Set(ch.substate);
            channel_instance_type_->Add(labels).Set(ch.instance_type);
            channel_nettime_short_->Add(labels).Set(ch.nettime_short);
            channel_nettime_long_->Add(labels).Set(ch.nettime_long);
            channel_xmitq_time_short_->Add(labels).Set(ch.xmitq_time_short);
            channel_xmitq_time_long_->Add(labels).Set(ch.xmitq_time_long);
            channel_batch_size_short_->Add(labels).Set(ch.batch_size_short);
            channel_batch_size_long_->Add(labels).Set(ch.batch_size_long);
            channel_max_instc_->Add(labels).Set(ch.max_instances);
            channel_cur_instc_->Add(labels).Set(ch.cur_sharing_convs);

            // Status squash: 0=OK(running/inactive), 1=transitioning, 2=stopped, 3=unknown
            int squash = 3;
            switch (ch.status) {
            case 0: squash = 0; break;  // inactive
            case 3: squash = 0; break;  // running
            case 1: squash = 1; break;  // binding
            case 2: squash = 1; break;  // starting
            case 4: squash = 2; break;  // stopping
            case 5: squash = 2; break;  // retrying
            case 6: squash = 2; break;  // stopped
            case 7: squash = 1; break;  // requesting
            case 8: squash = 2; break;  // paused
            case 13: squash = 1; break; // initializing
            default: squash = 3; break;
            }
            channel_status_squash_->Add(labels).Set(squash);
        }
    }
}

void MetricsCollector::collect_topic_status() {
    auto patterns = config_.collector.monitored_topics;
    if (patterns.empty()) return; // Don't query topics by default

    auto plat = mq_client_->get_platform_string();

    for (const auto& pattern : patterns) {
        auto topics = mq_client_->get_topic_status(pattern);
        for (const auto& t : topics) {
            auto labels = add_meta_labels({
                {"qmgr", config_.mq.queue_manager},
                {"platform", plat},
                {"topic", t.topic_string.empty() ? t.topic_name : t.topic_string},
                {"type", std::to_string(t.topic_type)}});

            topic_pub_count_->Add(labels).Set(t.pub_count);
            topic_sub_count_->Add(labels).Set(t.sub_count);
        }
    }
}

void MetricsCollector::collect_sub_status() {
    auto patterns = config_.collector.monitored_subscriptions;
    if (patterns.empty()) return;

    auto plat = mq_client_->get_platform_string();

    for (const auto& pattern : patterns) {
        auto subs = mq_client_->get_sub_status(pattern);
        for (const auto& s : subs) {
            auto labels = add_meta_labels({
                {"qmgr", config_.mq.queue_manager},
                {"platform", plat},
                {"subscription", s.sub_name},
                {"subid", s.sub_id},
                {"topic", s.topic_string},
                {"type", std::to_string(s.sub_type)}});

            sub_durable_->Add(labels).Set(s.durable);
            sub_type_->Add(labels).Set(s.sub_type);
            sub_message_count_->Add(labels).Set(s.message_count);
        }
    }
}

void MetricsCollector::collect_qmgr_status() {
    auto status = mq_client_->get_qmgr_status();
    auto plat = mq_client_->get_platform_string();

    if (status) {
        auto labels = add_meta_labels({
            {"qmgr", config_.mq.queue_manager},
            {"platform", plat},
            {"description", status->description}});

        qmgr_status_->Add(labels).Set(status->status);
        qmgr_connection_count_->Add(labels).Set(status->connection_count);
        qmgr_chinit_status_->Add(labels).Set(status->chinit_status);
        qmgr_cmd_server_status_->Add(labels).Set(status->cmd_server_status);

        // Compute uptime from start_date (YYYY-MM-DD) and start_time (HH.MM.SS)
        if (!status->start_date.empty() && !status->start_time.empty()) {
            std::tm tm_start = {};
            std::string dt_str = status->start_date + " " + status->start_time;
            std::istringstream iss(dt_str);
            iss >> std::get_time(&tm_start, "%Y-%m-%d %H.%M.%S");
            if (!iss.fail()) {
                auto start_epoch = std::mktime(&tm_start);
                if (start_epoch >= 0) {
                    auto now_epoch = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                    double uptime = std::difftime(now_epoch, start_epoch);
                    if (uptime > 0) qmgr_uptime_->Add(labels).Set(uptime);
                }
            }
        }
    } else {
        // Report disconnected status
        auto labels = add_meta_labels({
            {"qmgr", config_.mq.queue_manager},
            {"platform", plat},
            {"description", ""}});
        qmgr_status_->Add(labels).Set(0);
    }
}

void MetricsCollector::collect_cluster_status() {
    auto clusters = mq_client_->get_cluster_status();
    auto plat = mq_client_->get_platform_string();

    for (const auto& cl : clusters) {
        auto labels = add_meta_labels({
            {"qmgr", config_.mq.queue_manager},
            {"platform", plat},
            {"cluster", cl.cluster_name},
            {"qmtype", std::to_string(cl.qm_type)}});
        cluster_qmgr_status_->Add(labels).Set(cl.status);
        cluster_qmgr_suspend_->Add(labels).Set(cl.suspend);
    }
}

void MetricsCollector::collect_usage_status() {
    auto plat = mq_client_->get_platform_string();

    // Buffer pools
    auto bps = mq_client_->get_usage_bp_status();
    for (const auto& bp : bps) {
        auto labels = add_meta_labels({
            {"qmgr", config_.mq.queue_manager},
            {"platform", plat},
            {"bufferpool", std::to_string(bp.buffer_pool)},
            {"location", std::to_string(bp.location)},
            {"pageclass", std::to_string(bp.page_class)}});
        usage_bp_free_->Add(labels).Set(bp.free_buffers);
        usage_bp_total_->Add(labels).Set(bp.total_buffers);
    }

    // Page sets
    auto pss = mq_client_->get_usage_ps_status();
    for (const auto& ps : pss) {
        auto labels = add_meta_labels({
            {"qmgr", config_.mq.queue_manager},
            {"platform", plat},
            {"pageset", std::to_string(ps.pageset_id)},
            {"bufferpool", std::to_string(ps.buffer_pool)}});
        usage_ps_total_->Add(labels).Set(ps.total_pages);
        usage_ps_unused_->Add(labels).Set(ps.unused_pages);
    }
}

void MetricsCollector::set_baseline_metrics(int /*stats_count*/, int /*acct_count*/) {
    auto now = std::chrono::system_clock::now();
    auto epoch = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    last_collection_time_->Add(add_meta_labels({{"queue_manager", config_.mq.queue_manager}}))
        .Set(static_cast<double>(epoch));

    collection_info_->Add(add_meta_labels({{"queue_manager", config_.mq.queue_manager},
                           {"channel", config_.mq.channel},
                           {"platform", mq_client_->get_platform_string()},
                           {"collector_version", "1.0.0"}})).Set(1);
}

void MetricsCollector::collect_publication_metrics() {
    auto pubs = resource_monitor_->process_publications();
    if (pubs.empty()) return;

    const auto& ns = config_.prometheus.metrics_namespace;
    const auto& sub = config_.prometheus.subsystem;
    auto prefix = ns + (sub.empty() ? "" : "_" + sub);
    auto plat = mq_client_->get_platform_string();

    for (const auto& pm : pubs) {
        if (pm.object_name.empty()) {
            // QM-level metric (STATMQI, CPU, DISK, etc.)
            std::string key = pm.class_name + "_" + pm.metric_name;
            auto it = pub_qmgr_metrics_.find(key);
            if (it == pub_qmgr_metrics_.end()) {
                auto* family = &prometheus::BuildGauge()
                    .Name(prefix + "_qmgr_" + pm.metric_name)
                    .Help("QM-level resource monitor: " + pm.metric_name + " (" + pm.class_name + "/" + pm.type_name + ")")
                    .Register(*registry_);
                pub_qmgr_metrics_[key] = family;
                it = pub_qmgr_metrics_.find(key);
            }
            it->second->Add(add_meta_labels({
                {"qmgr", config_.mq.queue_manager},
                {"platform", plat},
                {"type", pm.type_name}
            })).Set(pm.value);
        } else {
            // Per-queue metric (STATQ)
            std::string key = pm.class_name + "_" + pm.metric_name;
            auto it = pub_queue_metrics_.find(key);
            if (it == pub_queue_metrics_.end()) {
                auto* family = &prometheus::BuildGauge()
                    .Name(prefix + "_queue_pub_" + pm.metric_name)
                    .Help("Per-queue resource monitor: " + pm.metric_name + " (" + pm.class_name + "/" + pm.type_name + ")")
                    .Register(*registry_);
                pub_queue_metrics_[key] = family;
                it = pub_queue_metrics_.find(key);
            }
            it->second->Add(add_meta_labels({
                {"qmgr", config_.mq.queue_manager},
                {"platform", plat},
                {"queue", pm.object_name}
            })).Set(pm.value);
        }
    }

    spdlog::info("Updated {} publication-based metrics", pubs.size());
}

void MetricsCollector::reset_metrics() {
    std::lock_guard lock(mu_);
    spdlog::info("Reset all metrics");
}

} // namespace ibmmq_exporter
