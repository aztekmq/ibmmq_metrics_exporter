#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "ibmmq_exporter/mqclient.h"

namespace ibmmq_exporter {

// A single measurable element within a monitor type
struct MonitorElement {
    int32_t     element_id{0};    // PCF parameter ID used in publications
    int32_t     datatype{0};      // UNIT=1, DELTA=2, etc.
    std::string description;      // e.g. "MQPUT count"
    std::string metric_name;      // e.g. "mqput_count" (derived from description)
};

// A type within a monitor class (e.g. PUT, GET, GENERAL)
struct MonitorType {
    int32_t     type_id{0};
    std::string type_name;        // e.g. "PUT", "GET", "GENERAL"
    std::string object_topic;     // data topic (may contain %s for per-object)
    std::vector<MonitorElement> elements;
};

// A monitor class (e.g. CPU, DISK, STATMQI, STATQ)
struct MonitorClass {
    int32_t     class_id{0};
    std::string class_name;       // e.g. "CPU", "DISK", "STATMQI", "STATQ"
    std::string topic_string;     // metadata types topic
    std::vector<MonitorType> types;
};

// A single metric extracted from a publication message
struct PublicationMetric {
    std::string class_name;
    std::string type_name;
    std::string object_name;      // queue name, or "" for QM-level
    std::string metric_name;
    double      value{0.0};       // normalized
    bool        is_delta{false};  // true for DELTA datatype (counters)
};

// Datatype constants from IBM MQ resource monitoring
namespace monitor_datatype {
    constexpr int32_t UNIT       = 1;
    constexpr int32_t DELTA      = 2;
    constexpr int32_t LSN        = 3;
    constexpr int32_t HUNDREDTHS = 100;
    constexpr int32_t KB         = 1024;
    constexpr int32_t PERCENT    = 10000;
    constexpr int32_t MICROSEC   = 1000000;
    constexpr int32_t MB         = 1048576;
    constexpr int32_t GB         = 100000000;
} // namespace monitor_datatype

// Undefine macros from IBM MQ headers that conflict with our constexpr declarations
#ifdef MQCA_TOPIC_STRING
#undef MQCA_TOPIC_STRING
#endif
#ifdef MQCA_Q_MGR_NAME
#undef MQCA_Q_MGR_NAME
#endif
#ifdef MQCA_Q_NAME
#undef MQCA_Q_NAME
#endif

// PCF parameter IDs for resource monitoring metadata
namespace monitor_pcf {
    constexpr int32_t MQIAMO_MONITOR_CLASS  = 839;
    constexpr int32_t MQIAMO_MONITOR_TYPE   = 840;
    constexpr int32_t MQIAMO_MONITOR_ELEMENT = 841;
    constexpr int32_t MQIAMO_MONITOR_DATATYPE = 842;
    constexpr int32_t MQIAMO_MONITOR_FLAGS  = 843;
    constexpr int32_t MQIAMO64_MONITOR_INTERVAL = 845;
    constexpr int32_t MQCAMO_MONITOR_CLASS  = 2713;
    constexpr int32_t MQCAMO_MONITOR_TYPE   = 2714;
    constexpr int32_t MQCAMO_MONITOR_DESC   = 2715;
    constexpr int32_t MQCA_TOPIC_STRING     = 2094;
    constexpr int32_t MQCA_Q_MGR_NAME       = 2015;
    constexpr int32_t MQCA_Q_NAME           = 2016;
} // namespace monitor_pcf

class ResourceMonitor {
public:
    ResourceMonitor(MQClient& client, const std::string& qmgr_name);
    ~ResourceMonitor();

    ResourceMonitor(const ResourceMonitor&) = delete;
    ResourceMonitor& operator=(const ResourceMonitor&) = delete;

    // Phase 1: Discover metadata from $SYS topics (one-time at startup)
    bool discover();

    // Phase 2: Create subscriptions to exact per-type data topics.
    // For per-queue classes (STATQ, STATAPP) pass discovered queue names.
    bool create_subscriptions(const std::vector<std::string>& queues = {});

    // Phase 3: Process pending publication messages (each collection cycle)
    std::vector<PublicationMetric> process_publications();

    // Cleanup subscriptions
    void close();

    [[nodiscard]] size_t class_count() const { return classes_.size(); }
    [[nodiscard]] const std::vector<MonitorClass>& classes() const { return classes_; }

private:
    // Parse PCF parameters from a publication message body
    struct PCFParam {
        int32_t type{0};
        int32_t param_id{0};
        int32_t int_value{0};
        int64_t int64_value{0};
        std::string str_value;
        std::vector<PCFParam> group_params;
    };
    std::vector<PCFParam> parse_pcf_params(const uint8_t* data, size_t len);

    // Discovery helpers
    bool discover_classes();
    bool discover_types(MonitorClass& cls);
    bool discover_elements(MonitorClass& cls, MonitorType& mtype,
                           const std::string& elements_topic);

    // Convert description to metric name
    static std::string description_to_metric_name(const std::string& desc);

    // Normalize a raw value based on datatype
    static double normalize_value(int64_t raw, int32_t datatype);

    // Extract object name from topic string
    static std::string extract_object_name(const std::string& topic_string,
                                           const std::string& class_name);

    MQClient&   client_;
    std::string qmgr_name_;

    std::vector<MonitorClass> classes_;

    // Per-type element lookup: key = "class_name/type_name"
    // value = map<element_id, MonitorElement pointer>
    std::map<std::string, std::map<int32_t, const MonitorElement*>> type_elements_;

    // Data subscription handles (managed by ResourceMonitor, not MQClient)
    struct DataSub {
        std::string class_name;
        std::string type_name;
        MQHOBJ hobj{0};
        MQHOBJ hsub{0};
    };
    std::vector<DataSub> data_subs_;
};

} // namespace ibmmq_exporter
