#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/registry.h>

#include "ibmmq_exporter/config.h"
#include "ibmmq_exporter/mqclient.h"
#include "ibmmq_exporter/pcf_parser.h"
#include "ibmmq_exporter/resource_monitor.h"

namespace ibmmq_exporter {

class MetricsCollector {
public:
    MetricsCollector(const Config& config,
                     MQClient& mq_client,
                     std::shared_ptr<prometheus::Registry> registry);

    // Perform a full metrics collection cycle
    void collect_metrics();

    // Reset all metrics
    void reset_metrics();

    // Update MQ client reference (for reconnection)
    void set_mq_client(MQClient& client) { mq_client_ = &client; }

    // Set resource monitor for $SYS publication-based metrics
    void set_resource_monitor(ResourceMonitor* monitor) { resource_monitor_ = monitor; }

    std::shared_ptr<prometheus::Registry> get_registry() const { return registry_; }

private:
    void init_metrics();

    // Message collection & processing
    std::vector<MQMessage> collect_messages(const std::string& queue_type);
    void update_metrics_from_messages(const std::vector<MQMessage>& stats_msgs,
                                     const std::vector<MQMessage>& acct_msgs);
    void process_statistics_message(const MQMessage& msg);
    void process_accounting_message(const MQMessage& msg);

    // Queue metric collection via MQINQ
    void collect_and_update_queue_metrics();
    void collect_and_update_handle_metrics();
    std::vector<std::string> get_queues_to_monitor();

    // Extended status collection via PCF
    void collect_channel_status();
    void collect_topic_status();
    void collect_sub_status();
    void collect_qmgr_status();
    void collect_cluster_status();
    void collect_usage_status();
    void collect_queue_online_status();

    void update_handle_metrics_from_statistics(const StatisticsData& stats);
    void set_baseline_metrics(int stats_count, int acct_count);

    // Update MQI statistics into Prometheus gauges
    void update_mqi_gauges(const MQIStatistics& mqi, const std::string& qmgr);

    // Process publication-based metrics from ResourceMonitor
    void collect_publication_metrics();

    // Metadata label helpers
    std::map<std::string, std::string> add_meta_labels(std::map<std::string, std::string> labels) const;

    const Config&      config_;
    MQClient*          mq_client_;
    PCFParser          pcf_parser_;
    ResourceMonitor*   resource_monitor_{nullptr};
    std::shared_ptr<prometheus::Registry> registry_;
    std::mutex         mu_;

    // Queue metrics
    prometheus::Family<prometheus::Gauge>* queue_depth_{nullptr};
    prometheus::Family<prometheus::Gauge>* queue_high_depth_{nullptr};
    prometheus::Family<prometheus::Gauge>* queue_enqueue_{nullptr};
    prometheus::Family<prometheus::Gauge>* queue_dequeue_{nullptr};
    prometheus::Family<prometheus::Gauge>* queue_input_handles_{nullptr};
    prometheus::Family<prometheus::Gauge>* queue_output_handles_{nullptr};
    prometheus::Family<prometheus::Gauge>* queue_readers_{nullptr};
    prometheus::Family<prometheus::Gauge>* queue_writers_{nullptr};
    prometheus::Family<prometheus::Gauge>* queue_process_{nullptr};

    prometheus::Family<prometheus::Gauge>* queue_max_depth_{nullptr};
    prometheus::Family<prometheus::Gauge>* queue_oldest_msg_age_{nullptr};
    prometheus::Family<prometheus::Gauge>* queue_uncommitted_msgs_{nullptr};
    prometheus::Family<prometheus::Gauge>* queue_qtime_short_{nullptr};
    prometheus::Family<prometheus::Gauge>* queue_qtime_long_{nullptr};
    prometheus::Family<prometheus::Gauge>* queue_qfile_current_size_{nullptr};
    prometheus::Family<prometheus::Gauge>* queue_qfile_max_size_{nullptr};

    // Per-queue per-app metrics
    prometheus::Family<prometheus::Gauge>* queue_app_puts_{nullptr};
    prometheus::Family<prometheus::Gauge>* queue_app_gets_{nullptr};
    prometheus::Family<prometheus::Gauge>* queue_app_msgs_received_{nullptr};
    prometheus::Family<prometheus::Gauge>* queue_app_msgs_sent_{nullptr};

    // Handle-level metrics
    prometheus::Family<prometheus::Gauge>* queue_handle_count_{nullptr};
    prometheus::Family<prometheus::Gauge>* queue_handle_info_{nullptr};
    prometheus::Family<prometheus::Gauge>* queue_process_detail_{nullptr};

    // Channel metrics (from statistics messages)
    prometheus::Family<prometheus::Gauge>* channel_messages_{nullptr};
    prometheus::Family<prometheus::Gauge>* channel_bytes_{nullptr};
    prometheus::Family<prometheus::Gauge>* channel_batches_{nullptr};

    // Channel status metrics (from PCF inquiry)
    prometheus::Family<prometheus::Gauge>* channel_status_{nullptr};
    prometheus::Family<prometheus::Gauge>* channel_status_msgs_{nullptr};
    prometheus::Family<prometheus::Gauge>* channel_bytes_sent_{nullptr};
    prometheus::Family<prometheus::Gauge>* channel_bytes_received_{nullptr};
    prometheus::Family<prometheus::Gauge>* channel_buffers_sent_{nullptr};
    prometheus::Family<prometheus::Gauge>* channel_buffers_received_{nullptr};
    prometheus::Family<prometheus::Gauge>* channel_substate_{nullptr};
    prometheus::Family<prometheus::Gauge>* channel_instance_type_{nullptr};
    prometheus::Family<prometheus::Gauge>* channel_nettime_short_{nullptr};
    prometheus::Family<prometheus::Gauge>* channel_nettime_long_{nullptr};
    prometheus::Family<prometheus::Gauge>* channel_xmitq_time_short_{nullptr};
    prometheus::Family<prometheus::Gauge>* channel_xmitq_time_long_{nullptr};
    prometheus::Family<prometheus::Gauge>* channel_batch_size_short_{nullptr};
    prometheus::Family<prometheus::Gauge>* channel_batch_size_long_{nullptr};
    prometheus::Family<prometheus::Gauge>* channel_max_instc_{nullptr};
    prometheus::Family<prometheus::Gauge>* channel_cur_instc_{nullptr};
    prometheus::Family<prometheus::Gauge>* channel_status_squash_{nullptr};

    // Topic status metrics
    prometheus::Family<prometheus::Gauge>* topic_pub_count_{nullptr};
    prometheus::Family<prometheus::Gauge>* topic_sub_count_{nullptr};

    // Subscription status metrics
    prometheus::Family<prometheus::Gauge>* sub_durable_{nullptr};
    prometheus::Family<prometheus::Gauge>* sub_type_{nullptr};
    prometheus::Family<prometheus::Gauge>* sub_message_count_{nullptr};

    // QM status metrics
    prometheus::Family<prometheus::Gauge>* qmgr_status_{nullptr};
    prometheus::Family<prometheus::Gauge>* qmgr_connection_count_{nullptr};
    prometheus::Family<prometheus::Gauge>* qmgr_chinit_status_{nullptr};
    prometheus::Family<prometheus::Gauge>* qmgr_cmd_server_status_{nullptr};
    prometheus::Family<prometheus::Gauge>* qmgr_uptime_{nullptr};

    // Cluster metrics
    prometheus::Family<prometheus::Gauge>* cluster_qmgr_status_{nullptr};
    prometheus::Family<prometheus::Gauge>* cluster_qmgr_suspend_{nullptr};

    // z/OS Buffer Pool metrics
    prometheus::Family<prometheus::Gauge>* usage_bp_free_{nullptr};
    prometheus::Family<prometheus::Gauge>* usage_bp_total_{nullptr};

    // z/OS Page Set metrics
    prometheus::Family<prometheus::Gauge>* usage_ps_total_{nullptr};
    prometheus::Family<prometheus::Gauge>* usage_ps_unused_{nullptr};

    // MQI metrics (map-based for all operations)
    std::map<std::string, prometheus::Family<prometheus::Gauge>*> mqi_metrics_;

    // Collection info
    prometheus::Family<prometheus::Gauge>* collection_info_{nullptr};
    prometheus::Family<prometheus::Gauge>* last_collection_time_{nullptr};
    prometheus::Family<prometheus::Gauge>* publications_count_{nullptr};

    // Publication-based metric families (dynamically keyed by class_name + metric_name)
    // QM-level: ibmmq_qmgr_<metric_name> with labels {qmgr, platform, type}
    // Per-queue: ibmmq_queue_pub_<metric_name> with labels {qmgr, platform, queue}
    std::map<std::string, prometheus::Family<prometheus::Gauge>*> pub_qmgr_metrics_;
    std::map<std::string, prometheus::Family<prometheus::Gauge>*> pub_queue_metrics_;

    // Cache of process info per queue
    std::map<std::string, std::vector<ProcInfo>> queue_procs_cache_;

    // Cached queue list per collection cycle (avoid redundant PCF discovery)
    std::vector<std::string> cached_queues_;
};

} // namespace ibmmq_exporter
