#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

extern "C" {
#include <cmqc.h>
#include <cmqxc.h>
}

#include "ibmmq_exporter/config.h"
#include "ibmmq_exporter/pcf_inquiry.h"

namespace ibmmq_exporter {

// Represents a message retrieved from IBM MQ
struct MQMessage {
    std::vector<uint8_t> data;
    std::string          type; // "stats" or "accounting"
    std::string          put_date;
    std::string          put_time;
    int32_t              msg_type{0};
    std::string          format;
    std::vector<uint8_t> msg_id;

    [[nodiscard]] bool is_statistics() const { return type == "stats"; }
    [[nodiscard]] bool is_accounting() const { return type == "accounting"; }
};

// Queue statistics retrieved via MQINQ
struct QueueStats {
    std::string queue_name;
    int32_t     current_depth{0};
    int32_t     open_input_count{0};
    int32_t     open_output_count{0};
};

// Detailed queue info via MQINQ
struct QueueInfo {
    std::string queue_name;
    int32_t     current_depth{0};
    int32_t     open_input_count{0};
    int32_t     open_output_count{0};
    int32_t     max_queue_depth{0};
};

// Handle/process information for a queue
struct HandleInfo {
    std::string application_name;
    std::string application_tag;
    int32_t     process_id{0};
    std::string user_identifier;
    std::string channel_name;
    std::string connection_name;
    std::string handle_state;
    std::string open_mode;
    std::string queue_name;
    int32_t     input_handle_count{0};
    int32_t     output_handle_count{0};
};

// Check if a queue name matches any exclusion pattern
bool queue_matches_exclusion(const std::string& queue_name,
                             const std::vector<std::string>& patterns);

class MQClient {
public:
    explicit MQClient(const MQConfig& config);
    ~MQClient();

    MQClient(const MQClient&) = delete;
    MQClient& operator=(const MQClient&) = delete;

    void connect();
    void disconnect();
    [[nodiscard]] bool is_connected() const { return connected_; }

    void open_stats_queue(const std::string& queue_name);
    void open_accounting_queue(const std::string& queue_name);

    // Get a single message; returns nullopt when no message available
    std::optional<MQMessage> get_message(const std::string& queue_type);
    // Drain all available messages from a queue
    std::vector<MQMessage> get_all_messages(const std::string& queue_type);

    // Queue inquiries
    std::optional<QueueStats> get_queue_stats(const std::string& queue_name);
    std::optional<QueueInfo>  get_queue_info(const std::string& queue_name);
    std::vector<HandleInfo>   get_queue_handles(const std::string& queue_name);
    std::vector<HandleInfo>   get_queue_handle_details_by_pcf(const std::string& queue_name);

    // PCF status inquiries
    std::vector<ChannelStatusDetails> get_channel_status(const std::string& pattern);
    std::vector<TopicStatusDetails>   get_topic_status(const std::string& pattern);
    std::vector<SubStatusDetails>     get_sub_status(const std::string& pattern);
    std::optional<QMgrStatusDetails>  get_qmgr_status();
    std::vector<ClusterQMgrDetails>   get_cluster_status();
    std::vector<UsageBPDetails>       get_usage_bp_status();
    std::vector<UsagePSDetails>       get_usage_ps_status();
    std::vector<QueueOnlineStatus>    get_queue_online_status(const std::string& queue_name);
    bool reset_queue_stats(const std::string& queue_name);

    // Queue discovery via PCF
    std::vector<std::string> discover_queues(const std::string& pattern);

    // Topic subscription ($SYS/MQ topics)
    bool subscribe_to_topic(const std::string& topic_string);
    std::vector<MQMessage> receive_publications();
    void unsubscribe_all();

    // Subscribe to a topic, read one retained message, then close the subscription.
    // Used for metadata discovery from $SYS topics.
    std::optional<MQMessage> subscribe_and_get(const std::string& topic_string);

    // Caller-managed subscriptions (for ResourceMonitor).
    // create_subscription creates a non-durable subscription with its own
    // EXPORTER.PUB.* queue and returns handles to the caller.
    bool create_subscription(const std::string& topic_string,
                             MQHOBJ& out_hobj, MQHOBJ& out_hsub);
    std::vector<MQMessage> get_messages_from_handle(MQHOBJ hobj, int max_messages = 500);
    void close_subscription(MQHOBJ& hsub, MQHOBJ& hobj);

    // Platform detection
    int32_t get_platform() const { return platform_; }
    std::string get_platform_string() const;

private:
    MQConfig config_;
    MQHCONN  hconn_{0};
    MQHOBJ   stats_queue_{0};
    MQHOBJ   acct_queue_{0};
    bool     connected_{false};
    bool     stats_open_{false};
    bool     acct_open_{false};
    int32_t  platform_{0};

    // Subscription handles (real MQ uses MQHOBJ for both)
    struct SubHandle {
        MQHOBJ hobj{0};
        MQHOBJ hsub{0};
    };
    std::vector<SubHandle> subscriptions_;

    MQHOBJ open_queue(const std::string& queue_name, MQLONG options);
    MQHOBJ open_queue(const std::string& queue_name, MQLONG options,
                      const std::string& dynamic_q_name, std::string& resolved_name);
    void   close_queue(MQHOBJ& hobj);

    // PCF command send/receive (creates a fresh reply queue per command)
    std::vector<std::vector<uint8_t>> send_pcf_command(const std::vector<uint8_t>& cmd);

    // Detect remote QM platform via MQINQ
    void detect_platform();
};

} // namespace ibmmq_exporter
