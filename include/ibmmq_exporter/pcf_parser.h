#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ibmmq_exporter {

// Undefine macros from IBM MQ headers that conflict with our constexpr declarations.
// This allows pcf_parser.h to work regardless of include order with cmqc.h / cmqcfc.h.
#ifdef MQCFT_NONE
#undef MQCFT_NONE
#undef MQCFT_COMMAND
#undef MQCFT_RESPONSE
#undef MQCFT_INTEGER
#undef MQCFT_STRING
#undef MQCFT_INTEGER_LIST
#undef MQCFT_STRING_LIST
#undef MQCFT_GROUP
#undef MQCFT_STATISTICS
#undef MQCFT_ACCOUNTING
#undef MQCFT_INTEGER64
#undef MQCFT_INTEGER64_LIST
#undef MQCFT_COMMAND_XR
#endif

#ifdef MQIA_CURRENT_Q_DEPTH
#undef MQIA_CURRENT_Q_DEPTH
#undef MQIA_OPEN_INPUT_COUNT
#undef MQIA_OPEN_OUTPUT_COUNT
#undef MQIA_HIGH_Q_DEPTH
#undef MQIA_MSG_ENQ_COUNT
#undef MQIA_MSG_DEQ_COUNT
#undef MQIA_MAX_Q_DEPTH
#endif

// Additional macros from cmqc.h / cmqcfc.h that conflict with pcf:: constexpr names.
// Comprehensive list to handle any include order.
#ifdef MQCA_Q_NAME
#undef MQCA_Q_NAME
#endif
#ifdef MQCA_Q_MGR_NAME
#undef MQCA_Q_MGR_NAME
#endif
#ifdef MQIACH_MSGS
#undef MQIACH_MSGS
#endif
#ifdef MQIACH_BATCHES
#undef MQIACH_BATCHES
#endif
#ifdef MQCACH_CONNECTION_NAME
#undef MQCACH_CONNECTION_NAME
#endif
#ifdef MQIAMO_OPENS
#undef MQIAMO_OPENS
#endif
#ifdef MQIAMO_CLOSES
#undef MQIAMO_CLOSES
#endif
#ifdef MQIAMO_PUTS
#undef MQIAMO_PUTS
#endif
#ifdef MQIAMO_GETS
#undef MQIAMO_GETS
#endif
#ifdef MQIAMO_COMMITS
#undef MQIAMO_COMMITS
#endif
#ifdef MQIAMO_BACKOUTS
#undef MQIAMO_BACKOUTS
#endif
#ifdef MQIAMO_BROWSES
#undef MQIAMO_BROWSES
#endif
#ifdef MQIAMO_INQS
#undef MQIAMO_INQS
#endif
#ifdef MQIAMO_SETS
#undef MQIAMO_SETS
#endif
#ifdef MQIAMO_FULL_BATCHES
#undef MQIAMO_FULL_BATCHES
#endif
#ifdef MQIAMO_INCOMPLETE_BATCHES
#undef MQIAMO_INCOMPLETE_BATCHES
#endif
#ifdef MQIACF_PROCESS_ID
#undef MQIACF_PROCESS_ID
#endif
#ifdef MQCACF_APPL_NAME
#undef MQCACF_APPL_NAME
#endif
#ifdef MQCACF_APPL_TAG
#undef MQCACF_APPL_TAG
#endif
#ifdef MQCACF_USER_IDENTIFIER
#undef MQCACF_USER_IDENTIFIER
#endif
#ifdef MQIAMO_MSGS_SENT
#undef MQIAMO_MSGS_SENT
#endif
#ifdef MQIAMO_MSGS_RCVD
#undef MQIAMO_MSGS_RCVD
#endif
#ifdef MQIAMO_BYTES_SENT
#undef MQIAMO_BYTES_SENT
#endif
#ifdef MQIAMO_MSG_BYTES_RCVD
#undef MQIAMO_MSG_BYTES_RCVD
#endif
#ifdef MQIACH_CHANNEL_STATUS
#undef MQIACH_CHANNEL_STATUS
#endif
#ifdef MQIACH_CHANNEL_TYPE
#undef MQIACH_CHANNEL_TYPE
#endif

// PCF constants
namespace pcf {
    // Format types
    constexpr int32_t MQCFT_NONE           = 0;
    constexpr int32_t MQCFT_COMMAND        = 1;
    constexpr int32_t MQCFT_RESPONSE       = 2;
    constexpr int32_t MQCFT_INTEGER        = 3;
    constexpr int32_t MQCFT_STRING         = 4;
    constexpr int32_t MQCFT_INTEGER_LIST   = 5;
    constexpr int32_t MQCFT_STRING_LIST    = 6;
    constexpr int32_t MQCFT_GROUP          = 20;
    constexpr int32_t MQCFT_STATISTICS     = 21;
    constexpr int32_t MQCFT_ACCOUNTING     = 22;
    constexpr int32_t MQCFT_INTEGER64      = 23;
    constexpr int32_t MQCFT_INTEGER64_LIST = 25;
    constexpr int32_t MQCFT_COMMAND_XR     = 16;

    // Command types (real IBM MQ 9.x values)
    constexpr int32_t CMD_INQUIRE_Q_MGR_STATUS = 161;
    constexpr int32_t CMD_STATISTICS_MQI       = 164;
    constexpr int32_t CMD_STATISTICS_Q         = 165;
    constexpr int32_t CMD_STATISTICS_CHANNEL   = 166;
    constexpr int32_t CMD_ACCOUNTING_MQI       = 167;
    constexpr int32_t CMD_ACCOUNTING_Q         = 168;

    // Parameter IDs
    constexpr int32_t MQCA_Q_NAME            = 2016;
    constexpr int32_t MQCA_Q_MGR_NAME        = 2015;
    constexpr int32_t MQCA_CHANNEL_NAME      = 3501;
    constexpr int32_t MQCA_CONNECTION_NAME   = 3502;
    constexpr int32_t MQCA_APPL_NAME         = 2024;
    constexpr int32_t MQIA_CURRENT_Q_DEPTH   = 3;
    constexpr int32_t MQIA_OPEN_INPUT_COUNT  = 17;
    constexpr int32_t MQIA_OPEN_OUTPUT_COUNT = 18;
    constexpr int32_t MQIA_HIGH_Q_DEPTH      = 36;
    constexpr int32_t MQIA_MSG_DEQ_COUNT     = 38;
    constexpr int32_t MQIA_MSG_ENQ_COUNT     = 37;

    // Channel statistics
    constexpr int32_t MQIACH_MSGS    = 1534;
    constexpr int32_t MQIACH_BYTES   = 1535;
    constexpr int32_t MQIACH_BATCHES = 1537;

    // MQI statistics (Windows values in 700+ range)
    constexpr int32_t MQIAMO_OPENS                = 733;
    constexpr int32_t MQIAMO_CLOSES               = 709;
    constexpr int32_t MQIAMO_PUTS                 = 735;
    constexpr int32_t MQIAMO_GETS                 = 722;
    constexpr int32_t MQIAMO_COMMITS              = 710;
    constexpr int32_t MQIAMO_BACKOUTS             = 704;
    constexpr int32_t MQIAMO_BROWSES              = 705;
    constexpr int32_t MQIAMO_INQS                 = 727;
    constexpr int32_t MQIAMO_SETS                 = 744;
    constexpr int32_t MQIAMO_DISC_CLOSE_TIMEOUT   = 765;
    constexpr int32_t MQIAMO_DISC_RESET_TIMEOUT   = 766;
    constexpr int32_t MQIAMO_FAILS                = 767;
    constexpr int32_t MQIAMO_INCOMPLETE_MSG       = 769;
    constexpr int32_t MQIAMO_WAIT_INTERVAL        = 770;
    constexpr int32_t MQIAMO_SYNCPOINT_HEURISTIC  = 771;
    constexpr int32_t MQIAMO_HEAPS                = 772;
    constexpr int32_t MQIAMO_LOGICAL_CONNECTIONS  = 773;
    constexpr int32_t MQIAMO_PHYSICAL_CONNECTIONS = 774;
    constexpr int32_t MQIAMO_CURRENT_CONNS        = 775;
    constexpr int32_t MQIAMO_PERSISTENT_MSGS      = 776;
    constexpr int32_t MQIAMO_NON_PERSISTENT_MSGS  = 777;
    constexpr int32_t MQIAMO_LONG_MSGS            = 778;
    constexpr int32_t MQIAMO_SHORT_MSGS           = 779;
    constexpr int32_t MQIAMO_QUEUE_TIME           = 781;
    constexpr int32_t MQIAMO_QUEUE_TIME_MAX       = 783;
    constexpr int32_t MQIAMO_ELAPSED_TIME         = 784;
    constexpr int32_t MQIAMO_ELAPSED_TIME_MAX     = 785;
    constexpr int32_t MQIAMO_CONN_TIME            = 786;
    constexpr int32_t MQIAMO_CONN_TIME_MAX        = 787;
    constexpr int32_t MQIAMO_STAMP_ENABLED        = 788;

    // MQI/channel activity monitoring (real IBM MQ 9.x values)
    constexpr int32_t MQIAMO_MSGS_RCVD          = 817;
    constexpr int32_t MQIAMO_MSGS_SENT          = 790;
    constexpr int32_t MQIAMO_MSG_BYTES_RCVD     = 818;
    constexpr int32_t MQIAMO_BYTES_SENT         = 791;
    constexpr int32_t MQIACH_CHANNEL_STATUS     = 1527;
    constexpr int32_t MQIACH_CHANNEL_TYPE       = 1511;

    // Accounting counters — no direct real MQIAMO_* equivalents for these
    // aggregated accounting fields.  Use the base MQI constants instead.
    constexpr int32_t MQIAMO_FULL_BATCHES    = 720;
    constexpr int32_t MQIAMO_INCOMPLETE_BATCHES = 726;

    constexpr int32_t MQCACF_APPL_NAME       = 3024;
    constexpr int32_t MQCACF_APPL_TAG        = 3058;
    constexpr int32_t MQCACF_USER_IDENTIFIER = 3025;
    constexpr int32_t MQCACH_CONNECTION_NAME  = 3506;
    constexpr int32_t MQIACF_PROCESS_ID      = 1024;
    constexpr int32_t MQCACF_COMMAND_TIME    = 3603;
} // namespace pcf

// Forward declarations for nested parameter type
struct PCFParameter;
using PCFParameterList = std::vector<PCFParameter>;

// Value held in a PCF parameter
using PCFValue = std::variant<
    std::monostate,         // empty
    int32_t,                // single int
    int64_t,                // single int64
    std::string,            // string
    std::vector<int64_t>,   // int list
    std::vector<std::string>, // string list
    PCFParameterList        // nested group
>;

struct PCFParameter {
    int32_t  parameter{0};
    int32_t  type{0};
    PCFValue value;
};

struct PCFHeader {
    int32_t type{0};
    int32_t struc_length{0};
    int32_t version{0};
    int32_t command{0};
    int32_t msg_seq_number{0};
    int32_t control{0};
    int32_t comp_code{0};
    int32_t reason{0};
    int32_t parameter_count{0};
};

// Process info associated with a queue
struct ProcInfo {
    std::string application_name;
    std::string application_tag;
    int32_t     process_id{0};
    std::string connection_name;
    std::string user_identifier;
    std::string channel_name;
    std::string role; // "input", "output", "unknown"
};

struct QueueStatistics {
    std::string          queue_name;
    int32_t              current_depth{0};
    int32_t              high_depth{0};
    int32_t              input_count{0};
    int32_t              output_count{0};
    int32_t              enqueue_count{0};
    int32_t              dequeue_count{0};
    bool                 has_readers{false};
    bool                 has_writers{false};
    std::vector<ProcInfo> associated_procs;
};

struct ChannelStatistics {
    std::string channel_name;
    std::string connection_name;
    int32_t     messages{0};
    int64_t     bytes{0};
    int32_t     batches{0};
};

struct MQIStatistics {
    std::string application_name;
    std::string application_tag;
    std::string connection_name;
    std::string user_identifier;
    std::string channel_name;
    int32_t opens{0}, closes{0}, puts{0}, gets{0};
    int32_t commits{0}, backouts{0}, browses{0};
    int32_t inqs{0}, sets{0};
    int32_t disc_close_timeout{0}, disc_reset_timeout{0};
    int32_t fails{0}, incomplete_batch{0}, incomplete_msg{0};
    int32_t wait_interval{0}, syncpoint_heuristic{0};
    int32_t heaps{0}, logical_connections{0}, physical_connections{0};
    int32_t current_conns{0};
    int32_t persistent_msgs{0}, non_persistent_msgs{0};
    int32_t long_msgs{0}, short_msgs{0};
    int64_t queue_time{0}, queue_time_max{0};
    int64_t elapsed_time{0}, elapsed_time_max{0};
    int64_t conn_time{0}, conn_time_max{0};
    int32_t stamp_enabled{0};
    int32_t msgs_received{0}, msgs_sent{0};
    int64_t bytes_received{0}, bytes_sent{0};
    int32_t channel_status{0}, channel_type{0};
    int32_t channel_errors{0}, channel_disc_count{0};
    int32_t channel_exit_name{0};
    int64_t backout_count{0}, commits_count{0}, rollback_count{0};
    int32_t full_batches{0}, partial_batches{0};
};

struct ConnectionInfo {
    std::string channel_name;
    std::string connection_name;
    std::string application_name;
    std::string application_tag;
    std::string user_identifier;
};

struct OperationCounts {
    int32_t gets{0}, puts{0}, browses{0};
    int32_t opens{0}, closes{0};
    int32_t commits{0}, backouts{0};
};

struct QueueAppOperation {
    std::string queue_name;
    std::string application_name;
    std::string connection_name;
    std::string user_identifier;
    int32_t puts{0}, gets{0};
    int32_t msgs_received{0}, msgs_sent{0};
};

struct StatisticsData {
    std::string                        type;
    std::string                        queue_manager;
    std::optional<QueueStatistics>     queue_stats;
    std::optional<ChannelStatistics>   channel_stats;
    std::optional<MQIStatistics>       mqi_stats;
};

struct AccountingData {
    std::string                        type;
    std::string                        queue_manager;
    std::optional<ConnectionInfo>      connection_info;
    std::optional<OperationCounts>     operations;
    std::vector<QueueAppOperation>     queue_operations;
};

// Parsed result from a PCF message
using ParsedMessage = std::variant<StatisticsData, AccountingData>;

class PCFParser {
public:
    PCFParser() = default;

    // Parse a PCF message and return structured data
    std::optional<ParsedMessage> parse_message(const std::vector<uint8_t>& data,
                                               const std::string& msg_type);

private:
    PCFHeader                parse_header(const uint8_t* data, size_t len);
    std::vector<PCFParameter> parse_parameters(const uint8_t* data, size_t len);

    StatisticsData  parse_statistics(const PCFHeader& hdr, const std::vector<PCFParameter>& params);
    AccountingData  parse_accounting(const PCFHeader& hdr, const std::vector<PCFParameter>& params);

    QueueStatistics   parse_queue_stats(const std::vector<PCFParameter>& params);
    ChannelStatistics parse_channel_stats(const std::vector<PCFParameter>& params);
    MQIStatistics     parse_mqi_stats(const std::vector<PCFParameter>& params);
    ConnectionInfo    parse_connection_info(const std::vector<PCFParameter>& params);
    OperationCounts   parse_operation_counts(const std::vector<PCFParameter>& params);

    static std::string clean_string(const std::string& s);
};

} // namespace ibmmq_exporter
