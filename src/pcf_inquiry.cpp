#include "ibmmq_exporter/pcf_inquiry.h"

#include <algorithm>
#include <cstring>

#include <spdlog/spdlog.h>

namespace ibmmq_exporter {

// --- Low-level helpers ---
// All integer read/write uses native (platform) byte order.
// MQ handles encoding conversion via MQMD.Encoding and MQGMO_CONVERT.

void PCFInquiry::append_int32(std::vector<uint8_t>& buf, int32_t value) {
    auto p = reinterpret_cast<const uint8_t*>(&value);
    buf.insert(buf.end(), p, p + 4);
}

static uint32_t read_int32(const uint8_t* p) {
    uint32_t val;
    std::memcpy(&val, p, 4);
    return val;
}

static int64_t read_int64(const uint8_t* p) {
    int64_t val;
    std::memcpy(&val, p, 8);
    return val;
}

static std::string trim_mq_string(const std::string& s) {
    auto pos = s.find_last_not_of(std::string("\0 ", 2));
    if (pos != std::string::npos) return s.substr(0, pos + 1);
    return {};
}

// --- Generic PCF builders ---

// MQCFH layout (36 bytes, 9 x MQLONG):
//   offset  0: Type
//   offset  4: StrucLength
//   offset  8: Version
//   offset 12: Command
//   offset 16: MsgSeqNumber
//   offset 20: Control
//   offset 24: CompCode
//   offset 28: Reason
//   offset 32: ParameterCount
constexpr size_t MQCFH_SIZE = 36;

std::vector<uint8_t> PCFInquiry::build_pcf_cmd(int32_t command, int32_t param_count) {
    std::vector<uint8_t> buf;
    buf.reserve(512);
    append_int32(buf, 1);              // Type = MQCFT_COMMAND
    append_int32(buf, MQCFH_SIZE);     // StrucLength = 36 bytes
    append_int32(buf, 1);              // Version = 1
    append_int32(buf, command);
    append_int32(buf, 1);              // MsgSeqNumber
    append_int32(buf, 1);              // Control = MQCFC_LAST
    append_int32(buf, 0);              // CompCode
    append_int32(buf, 0);              // Reason
    append_int32(buf, param_count);    // ParameterCount
    return buf;
}

// MQCFST layout:
//   offset  0: Type (4) = MQCFT_STRING
//   offset  4: StrucLength (4)
//   offset  8: Parameter (4)
//   offset 12: CodedCharSetId (4)
//   offset 16: StringLength (4)
//   offset 20: String[N] (padded to 4-byte boundary)
void PCFInquiry::append_string_param(std::vector<uint8_t>& buf, int32_t param_id, const std::string& value) {
    size_t param_start = buf.size();
    append_int32(buf, 4);          // Type = MQCFT_STRING
    size_t len_pos = buf.size();
    append_int32(buf, 0);          // placeholder for StrucLength
    append_int32(buf, param_id);   // Parameter
    append_int32(buf, 0);          // CodedCharSetId = MQCCSI_DEFAULT

    auto str_len = static_cast<int32_t>(value.size());
    append_int32(buf, str_len);    // StringLength
    buf.insert(buf.end(), value.begin(), value.end());

    int padding = (4 - (str_len % 4)) % 4;
    for (int i = 0; i < padding; ++i) buf.push_back(0);

    // Patch StrucLength in native byte order
    auto param_len = static_cast<int32_t>(buf.size() - param_start);
    std::memcpy(&buf[len_pos], &param_len, 4);
}

// MQCFIN layout:
//   offset  0: Type (4) = MQCFT_INTEGER
//   offset  4: StrucLength (4) = 16
//   offset  8: Parameter (4)
//   offset 12: Value (4)
void PCFInquiry::append_integer_param(std::vector<uint8_t>& buf, int32_t param_id, int32_t value) {
    append_int32(buf, 3);          // Type = MQCFT_INTEGER
    append_int32(buf, 16);         // StrucLength
    append_int32(buf, param_id);   // Parameter
    append_int32(buf, value);      // Value
}

// --- Command builders ---

std::vector<uint8_t> PCFInquiry::build_inquire_q_cmd(const std::string& queue_name) {
    auto buf = build_pcf_cmd(13, 2); // MQCMD_INQUIRE_Q = 13, 2 params
    append_string_param(buf, 2016, queue_name); // MQCA_Q_NAME
    append_integer_param(buf, 20, 1); // MQIA_Q_TYPE = MQQT_LOCAL (exclude model/alias/remote)
    spdlog::debug("Built INQUIRE_Q PCF command for {}, size={}", queue_name, buf.size());
    return buf;
}

std::vector<uint8_t> PCFInquiry::build_inquire_q_status_cmd(const std::string& queue_name) {
    auto buf = build_pcf_cmd(41, 2); // MQCMD_INQUIRE_Q_STATUS = 41
    append_string_param(buf, 2016, queue_name); // MQCA_Q_NAME
    append_integer_param(buf, 1103, 1104); // MQIACF_Q_STATUS_TYPE = MQIACF_Q_HANDLE
    spdlog::debug("Built INQUIRE_Q_STATUS PCF command for {}, size={}", queue_name, buf.size());
    return buf;
}

std::vector<uint8_t> PCFInquiry::build_inquire_channel_status_cmd(const std::string& channel_pattern) {
    auto buf = build_pcf_cmd(42, 1); // MQCMD_INQUIRE_CHANNEL_STATUS = 42
    append_string_param(buf, 3501, channel_pattern); // MQCACH_CHANNEL_NAME
    spdlog::debug("Built INQUIRE_CHANNEL_STATUS for {}, size={}", channel_pattern, buf.size());
    return buf;
}

std::vector<uint8_t> PCFInquiry::build_inquire_topic_status_cmd(const std::string& topic_pattern) {
    auto buf = build_pcf_cmd(183, 1); // MQCMD_INQUIRE_TOPIC_STATUS = 183
    append_string_param(buf, 2094, topic_pattern); // MQCA_TOPIC_STRING
    spdlog::debug("Built INQUIRE_TOPIC_STATUS for {}, size={}", topic_pattern, buf.size());
    return buf;
}

std::vector<uint8_t> PCFInquiry::build_inquire_sub_status_cmd(const std::string& sub_pattern) {
    auto buf = build_pcf_cmd(182, 1); // MQCMD_INQUIRE_SUB_STATUS = 182
    append_string_param(buf, 3152, sub_pattern); // MQCACF_SUB_NAME
    spdlog::debug("Built INQUIRE_SUB_STATUS for {}, size={}", sub_pattern, buf.size());
    return buf;
}

std::vector<uint8_t> PCFInquiry::build_inquire_qmgr_status_cmd() {
    auto buf = build_pcf_cmd(161, 0); // MQCMD_INQUIRE_Q_MGR_STATUS = 161
    spdlog::debug("Built INQUIRE_Q_MGR_STATUS, size={}", buf.size());
    return buf;
}

std::vector<uint8_t> PCFInquiry::build_inquire_cluster_qmgr_cmd() {
    auto buf = build_pcf_cmd(70, 1); // MQCMD_INQUIRE_CLUSTER_Q_MGR = 70
    append_string_param(buf, 2029, "*"); // MQCA_CLUSTER_NAME = wildcard
    spdlog::debug("Built INQUIRE_CLUSTER_Q_MGR, size={}", buf.size());
    return buf;
}

std::vector<uint8_t> PCFInquiry::build_inquire_usage_cmd(int32_t usage_type) {
    auto buf = build_pcf_cmd(126, 1); // MQCMD_INQUIRE_USAGE = 126
    append_integer_param(buf, 1157, usage_type); // MQIACF_USAGE_TYPE
    spdlog::debug("Built INQUIRE_USAGE type={}, size={}", usage_type, buf.size());
    return buf;
}

std::vector<uint8_t> PCFInquiry::build_inquire_q_status_online_cmd(const std::string& queue_name) {
    auto buf = build_pcf_cmd(41, 2); // MQCMD_INQUIRE_Q_STATUS = 41
    append_string_param(buf, 2016, queue_name); // MQCA_Q_NAME
    append_integer_param(buf, 1103, 1105); // MQIACF_Q_STATUS_TYPE = MQIACF_Q_STATUS (online)
    spdlog::debug("Built INQUIRE_Q_STATUS (online) for {}, size={}", queue_name, buf.size());
    return buf;
}

std::vector<uint8_t> PCFInquiry::build_reset_q_stats_cmd(const std::string& queue_name) {
    auto buf = build_pcf_cmd(17, 1); // MQCMD_RESET_Q_STATS
    append_string_param(buf, 2016, queue_name); // MQCA_Q_NAME
    spdlog::debug("Built RESET_Q_STATS for {}, size={}", queue_name, buf.size());
    return buf;
}

// --- Generic response parsing helpers ---

std::string PCFInquiry::read_string_param(const uint8_t* data, size_t len) {
    // data points past Type+StrucLength (i.e., at Parameter field)
    // Layout: Parameter(4) + CodedCharSetId(4) + StringLength(4) + String[N]
    if (len < 12) return {};
    uint32_t str_len = read_int32(data + 8);
    if (12 + str_len > len) return {};
    return trim_mq_string(std::string(reinterpret_cast<const char*>(data + 12), str_len));
}

int32_t PCFInquiry::read_int_param(const uint8_t* data, size_t len) {
    // data points past Type+StrucLength (i.e., at Parameter field)
    // Layout: Parameter(4) + Value(4)
    if (len < 8) return 0;
    return static_cast<int32_t>(read_int32(data + 4));
}

// Parse a single PCF response message, calling a visitor lambda per parameter.
// The visitor receives: (param_type, param_id, pointer_to_param_start, param_struct_length)
template <typename Visitor>
static void parse_pcf_response_params(const uint8_t* data, size_t len, Visitor&& visitor) {
    if (len < MQCFH_SIZE) return;

    // CompCode at offset 24, Reason at offset 28
    uint32_t comp_code = read_int32(data + 24);
    if (comp_code != 0) {
        spdlog::debug("PCF response error: comp_code={}, reason={}", comp_code, read_int32(data + 28));
        return;
    }

    size_t offset = MQCFH_SIZE; // parameters start after 36-byte header
    while (offset + 8 <= len) {
        uint32_t param_type = read_int32(data + offset);       // Type
        uint32_t param_len  = read_int32(data + offset + 4);   // StrucLength
        if (param_len == 0 || offset + param_len > len) break;

        uint32_t param_id = 0;
        if (offset + 12 <= len) param_id = read_int32(data + offset + 8); // Parameter

        visitor(param_type, param_id, data + offset, param_len);
        offset += param_len;
    }
}

// Helper: read a string value from a MQCFST parameter structure.
// pdata points to the start of the parameter (Type field).
// MQCFST: Type(4) StrucLength(4) Parameter(4) CodedCharSetId(4) StringLength(4) String[N]
//         0       4              8             12                16              20
static std::string read_pcf_string(const uint8_t* pdata, uint32_t plen) {
    if (plen < 24) return {};
    uint32_t str_len = read_int32(pdata + 16);
    if (20 + str_len > plen) str_len = plen - 20;
    return trim_mq_string(std::string(
        reinterpret_cast<const char*>(pdata + 20), str_len));
}

// Helper: read an integer value from a MQCFIN parameter structure.
// MQCFIN: Type(4) StrucLength(4) Parameter(4) Value(4)
//         0       4              8             12
static int32_t read_pcf_int32(const uint8_t* pdata, uint32_t plen) {
    if (plen < 16) return 0;
    return static_cast<int32_t>(read_int32(pdata + 12));
}

// Helper: read an int64 value from a MQCFIN64 parameter structure.
// MQCFIN64: Type(4) StrucLength(4) Parameter(4) Reserved(4) Value(8)
//           0       4              8             12          16
static int64_t read_pcf_int64(const uint8_t* pdata, uint32_t plen) {
    if (plen < 24) return 0;
    return read_int64(pdata + 16);
}

// Helper: read an integer list from a MQCFIL parameter structure.
// MQCFIL: Type(4) StrucLength(4) Parameter(4) Count(4) Values[N*4]
//         0       4              8             12       16
static std::vector<int32_t> read_pcf_int_list(const uint8_t* pdata, uint32_t plen) {
    std::vector<int32_t> values;
    if (plen < 16) return values;
    uint32_t count = read_int32(pdata + 12);
    if (16 + count * 4 > plen) count = (plen - 16) / 4;
    values.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        values.push_back(static_cast<int32_t>(read_int32(pdata + 16 + i * 4)));
    }
    return values;
}

// --- Channel status parser ---

std::vector<ChannelStatusDetails> PCFInquiry::parse_channel_status_response(
        const std::vector<std::vector<uint8_t>>& responses) {
    std::vector<ChannelStatusDetails> result;
    for (const auto& resp : responses) {
        if (resp.size() < MQCFH_SIZE) continue;
        ChannelStatusDetails ch;
        parse_pcf_response_params(resp.data(), resp.size(),
            [&](uint32_t ptype, uint32_t pid, const uint8_t* pdata, uint32_t plen) {
                if (ptype == 4) { // MQCFT_STRING
                    auto val = read_pcf_string(pdata, plen);
                    switch (pid) {
                    case 3501: ch.channel_name = val; break;    // MQCACH_CHANNEL_NAME
                    case 3506: ch.connection_name = val; break; // MQCACH_CONNECTION_NAME
                    case 3507: ch.remote_qmgr = val; break;    // MQCACH_MCA_NAME (remote partner)
                    case 3524: ch.last_msg_time = val; break;   // MQCACH_LAST_MSG_TIME
                    case 3525: ch.last_msg_date = val; break;   // MQCACH_LAST_MSG_DATE
                    case 3528: ch.start_time = val; break;      // MQCACH_CHANNEL_START_TIME
                    case 3529: ch.start_date = val; break;      // MQCACH_CHANNEL_START_DATE
                    case 3530: ch.job_name = val; break;        // MQCACH_MCA_JOB_NAME
                    case 3544: ch.ssl_cipher = val; break;      // MQCACH_SSL_CIPHER_SPEC
                    }
                } else if (ptype == 3) { // MQCFT_INTEGER
                    int32_t val = read_pcf_int32(pdata, plen);
                    switch (pid) {
                    case 1527: ch.status = val; break;          // MQIACH_CHANNEL_STATUS
                    case 1511: ch.channel_type = val; break;    // MQIACH_CHANNEL_TYPE
                    case 1534: ch.msgs = val; break;            // MQIACH_MSGS
                    case 1537: ch.batches = val; break;         // MQIACH_BATCHES
                    case 1609: ch.substate = val; break;        // MQIACH_CHANNEL_SUBSTATE
                    case 1523: ch.instance_type = val; break;   // MQIACH_CHANNEL_INSTANCE_TYPE
                    case 1538: ch.buffers_sent = val; break;    // MQIACH_BUFFERS_SENT
                    case 1539: ch.buffers_received = val; break;// MQIACH_BUFFERS_RCVD
                    case 1585: ch.cur_sharing_convs = val; break; // MQIACH_CURRENT_SHARING_CONVS
                    case 1618: ch.max_instances = val; break;   // MQIACH_MAX_INSTANCES
                    case 1619: ch.max_insts_per_client = val; break; // MQIACH_MAX_INSTS_PER_CLIENT
                    }
                } else if (ptype == 23) { // MQCFT_INTEGER64
                    int64_t val = read_pcf_int64(pdata, plen);
                    switch (pid) {
                    case 1535: ch.bytes_sent = val; break;      // MQIACH_BYTES_SENT
                    case 1536: ch.bytes_received = val; break;  // MQIACH_BYTES_RECEIVED
                    }
                } else if (ptype == 5) { // MQCFT_INTEGER_LIST
                    auto vals = read_pcf_int_list(pdata, plen);
                    switch (pid) {
                    case 1605: // MQIACH_NETWORK_TIME_INDICATOR
                        if (vals.size() >= 1) ch.nettime_short = vals[0];
                        if (vals.size() >= 2) ch.nettime_long = vals[1];
                        break;
                    case 1604: // MQIACH_XMITQ_TIME_INDICATOR
                        if (vals.size() >= 1) ch.xmitq_time_short = vals[0];
                        if (vals.size() >= 2) ch.xmitq_time_long = vals[1];
                        break;
                    case 1607: // MQIACH_BATCH_SIZE_INDICATOR
                        if (vals.size() >= 1) ch.batch_size_short = vals[0];
                        if (vals.size() >= 2) ch.batch_size_long = vals[1];
                        break;
                    }
                }
            });
        if (!ch.channel_name.empty()) result.push_back(std::move(ch));
    }
    spdlog::debug("Parsed {} channel status entries", result.size());
    return result;
}

// --- Topic status parser ---

std::vector<TopicStatusDetails> PCFInquiry::parse_topic_status_response(
        const std::vector<std::vector<uint8_t>>& responses) {
    std::vector<TopicStatusDetails> result;
    for (const auto& resp : responses) {
        if (resp.size() < MQCFH_SIZE) continue;
        TopicStatusDetails topic;
        parse_pcf_response_params(resp.data(), resp.size(),
            [&](uint32_t ptype, uint32_t pid, const uint8_t* pdata, uint32_t plen) {
                if (ptype == 4) {
                    auto val = read_pcf_string(pdata, plen);
                    switch (pid) {
                    case 2094: topic.topic_string = val; break;
                    case 2092: topic.topic_name = val; break;
                    }
                } else if (ptype == 3) {
                    int32_t val = read_pcf_int32(pdata, plen);
                    switch (pid) {
                    case 208: topic.topic_type = val; break;  // MQIA_TOPIC_TYPE
                    case 215: topic.pub_count = val; break;   // MQIA_PUB_COUNT
                    case 204: topic.sub_count = val; break;   // MQIA_SUB_COUNT
                    }
                }
            });
        if (!topic.topic_string.empty() || !topic.topic_name.empty())
            result.push_back(std::move(topic));
    }
    spdlog::debug("Parsed {} topic status entries", result.size());
    return result;
}

// --- Subscription status parser ---

std::vector<SubStatusDetails> PCFInquiry::parse_sub_status_response(
        const std::vector<std::vector<uint8_t>>& responses) {
    std::vector<SubStatusDetails> result;
    for (const auto& resp : responses) {
        if (resp.size() < MQCFH_SIZE) continue;
        SubStatusDetails sub;
        parse_pcf_response_params(resp.data(), resp.size(),
            [&](uint32_t ptype, uint32_t pid, const uint8_t* pdata, uint32_t plen) {
                if (ptype == 4) {
                    auto val = read_pcf_string(pdata, plen);
                    switch (pid) {
                    case 3152: sub.sub_name = val; break;      // MQCACF_SUB_NAME
                    case 2094: sub.topic_string = val; break;  // MQCA_TOPIC_STRING
                    case 3154: sub.destination = val; break;   // MQCACF_DESTINATION
                    case 7016: sub.sub_id = val; break;        // MQBACF_SUB_ID
                    case 3167: sub.last_msg_time = val; break; // MQCACF_LAST_MSG_TIME
                    case 3168: sub.last_msg_date = val; break; // MQCACF_LAST_MSG_DATE
                    }
                } else if (ptype == 3) {
                    int32_t val = read_pcf_int32(pdata, plen);
                    switch (pid) {
                    case 1289: sub.sub_type = val; break;      // MQIACF_SUB_TYPE
                    case 175:  sub.durable = val; break;       // MQIA_DURABLE_SUB
                    case 1290: sub.message_count = val; break; // MQIACF_MESSAGE_COUNT
                    }
                }
            });
        if (!sub.sub_name.empty()) result.push_back(std::move(sub));
    }
    spdlog::debug("Parsed {} subscription status entries", result.size());
    return result;
}

// --- QM status parser ---

std::vector<QMgrStatusDetails> PCFInquiry::parse_qmgr_status_response(
        const std::vector<std::vector<uint8_t>>& responses) {
    std::vector<QMgrStatusDetails> result;
    for (const auto& resp : responses) {
        if (resp.size() < MQCFH_SIZE) continue;
        QMgrStatusDetails qm;
        parse_pcf_response_params(resp.data(), resp.size(),
            [&](uint32_t ptype, uint32_t pid, const uint8_t* pdata, uint32_t plen) {
                if (ptype == 4) {
                    auto val = read_pcf_string(pdata, plen);
                    switch (pid) {
                    case 2015: qm.qmgr_name = val; break;     // MQCA_Q_MGR_NAME
                    case 2014: qm.description = val; break;    // MQCA_Q_MGR_DESC
                    case 3175: qm.start_date = val; break;     // MQCACF_Q_MGR_START_DATE
                    case 3176: qm.start_time = val; break;     // MQCACF_Q_MGR_START_TIME
                    }
                } else if (ptype == 3) {
                    int32_t val = read_pcf_int32(pdata, plen);
                    switch (pid) {
                    case 1149: qm.status = val; break;             // MQIACF_Q_MGR_STATUS
                    case 1232: qm.chinit_status = val; break;      // MQIACF_CHINIT_STATUS
                    case 1229: qm.connection_count = val; break;   // MQIACF_CONNECTION_COUNT
                    case 1233: qm.cmd_server_status = val; break;  // MQIACF_CMD_SERVER_STATUS
                    }
                }
            });
        result.push_back(std::move(qm));
    }
    spdlog::debug("Parsed {} QM status entries", result.size());
    return result;
}

// --- Cluster QM parser ---

std::vector<ClusterQMgrDetails> PCFInquiry::parse_cluster_qmgr_response(
        const std::vector<std::vector<uint8_t>>& responses) {
    std::vector<ClusterQMgrDetails> result;
    for (const auto& resp : responses) {
        if (resp.size() < MQCFH_SIZE) continue;
        ClusterQMgrDetails cl;
        parse_pcf_response_params(resp.data(), resp.size(),
            [&](uint32_t ptype, uint32_t pid, const uint8_t* pdata, uint32_t plen) {
                if (ptype == 4) {
                    auto val = read_pcf_string(pdata, plen);
                    switch (pid) {
                    case 2029: cl.cluster_name = val; break;   // MQCA_CLUSTER_NAME
                    case 2015: cl.qmgr_name = val; break;     // MQCA_Q_MGR_NAME
                    }
                } else if (ptype == 3) {
                    int32_t val = read_pcf_int32(pdata, plen);
                    switch (pid) {
                    case 125:  cl.qm_type = val; break;
                    case 1127: cl.status = val; break;
                    case 1087: cl.suspend = val; break;        // MQIACF_SUSPEND
                    }
                }
            });
        if (!cl.cluster_name.empty()) result.push_back(std::move(cl));
    }
    spdlog::debug("Parsed {} cluster QM entries", result.size());
    return result;
}

// --- Usage BP parser ---

std::vector<UsageBPDetails> PCFInquiry::parse_usage_bp_response(
        const std::vector<std::vector<uint8_t>>& responses) {
    std::vector<UsageBPDetails> result;
    for (const auto& resp : responses) {
        if (resp.size() < MQCFH_SIZE) continue;
        UsageBPDetails bp;
        parse_pcf_response_params(resp.data(), resp.size(),
            [&](uint32_t ptype, uint32_t pid, const uint8_t* pdata, uint32_t plen) {
                if (ptype == 3) {
                    int32_t val = read_pcf_int32(pdata, plen);
                    switch (pid) {
                    case 1158: bp.buffer_pool = val; break;    // MQIACF_BUFFER_POOL_ID
                    case 1330: bp.free_buffers = val; break;   // MQIACF_USAGE_FREE_BUFF
                    case 1166: bp.total_buffers = val; break;  // MQIACF_USAGE_TOTAL_BUFFERS
                    case 1170: bp.location = val; break;       // MQIACF_USAGE_BUFFER_POOL
                    }
                }
            });
        result.push_back(std::move(bp));
    }
    return result;
}

// --- Usage PS parser ---

std::vector<UsagePSDetails> PCFInquiry::parse_usage_ps_response(
        const std::vector<std::vector<uint8_t>>& responses) {
    std::vector<UsagePSDetails> result;
    for (const auto& resp : responses) {
        if (resp.size() < MQCFH_SIZE) continue;
        UsagePSDetails ps;
        parse_pcf_response_params(resp.data(), resp.size(),
            [&](uint32_t ptype, uint32_t pid, const uint8_t* pdata, uint32_t plen) {
                if (ptype == 3) {
                    int32_t val = read_pcf_int32(pdata, plen);
                    switch (pid) {
                    case 62:   ps.pageset_id = val; break;       // MQIA_PAGESET_ID
                    case 1170: ps.buffer_pool = val; break;      // MQIACF_USAGE_BUFFER_POOL
                    case 1159: ps.total_pages = val; break;      // MQIACF_USAGE_TOTAL_PAGES
                    case 1160: ps.unused_pages = val; break;     // MQIACF_USAGE_UNUSED_PAGES
                    case 1161: ps.persist_pages = val; break;    // MQIACF_USAGE_PERSIST_PAGES
                    case 1162: ps.nonpersist_pages = val; break; // MQIACF_USAGE_NONPERSIST_PAGES
                    case 1163: ps.restart_pages = val; break;    // MQIACF_USAGE_RESTART_EXTENTS
                    case 1164: ps.expand_count = val; break;     // MQIACF_USAGE_EXPAND_COUNT
                    }
                }
            });
        result.push_back(std::move(ps));
    }
    return result;
}

// --- Queue online status parser ---

std::vector<QueueOnlineStatus> PCFInquiry::parse_queue_online_status_response(
        const std::vector<std::vector<uint8_t>>& responses) {
    std::vector<QueueOnlineStatus> result;
    for (const auto& resp : responses) {
        if (resp.size() < MQCFH_SIZE) continue;
        QueueOnlineStatus qs;
        parse_pcf_response_params(resp.data(), resp.size(),
            [&](uint32_t ptype, uint32_t pid, const uint8_t* pdata, uint32_t plen) {
                if (ptype == 4) { // MQCFT_STRING
                    auto val = read_pcf_string(pdata, plen);
                    switch (pid) {
                    case 2016: qs.queue_name = val; break;      // MQCA_Q_NAME
                    case 3128: qs.last_put_date = val; break;   // MQCACF_LAST_PUT_DATE
                    case 3129: qs.last_put_time = val; break;   // MQCACF_LAST_PUT_TIME
                    case 3130: qs.last_get_date = val; break;   // MQCACF_LAST_GET_DATE
                    case 3131: qs.last_get_time = val; break;   // MQCACF_LAST_GET_TIME
                    }
                } else if (ptype == 3) { // MQCFT_INTEGER
                    int32_t val = read_pcf_int32(pdata, plen);
                    switch (pid) {
                    case 1227: qs.oldest_msg_age = val; break;      // MQIACF_OLDEST_MSG_AGE
                    case 1027: qs.uncommitted_msgs = val; break;    // MQIACF_UNCOMMITTED_MSGS
                    case 1437: qs.cur_q_file_size = val; break;     // MQIACF_CUR_Q_FILE_SIZE
                    case 1438: qs.cur_max_file_size = val; break;   // MQIACF_CUR_MAX_FILE_SIZE
                    }
                } else if (ptype == 5) { // MQCFT_INTEGER_LIST
                    auto vals = read_pcf_int_list(pdata, plen);
                    if (pid == 1226) { // MQIACF_Q_TIME_INDICATOR
                        if (vals.size() >= 1) qs.qtime_short = vals[0];
                        if (vals.size() >= 2) qs.qtime_long = vals[1];
                    }
                }
            });
        if (!qs.queue_name.empty()) result.push_back(std::move(qs));
    }
    spdlog::debug("Parsed {} queue online status entries", result.size());
    return result;
}

// --- Existing queue status response parser ---
// Used by discover_queues() and get_queue_handle_details_by_pcf()

void PCFInquiry::parse_string_param(const uint8_t* data, size_t len,
                                    QueueHandleDetails& handle) {
    // data points past Type+StrucLength (at Parameter field)
    // Layout: Parameter(4) + CodedCharSetId(4) + StringLength(4) + String[N]
    if (len < 12) return;
    uint32_t param_id = read_int32(data);
    uint32_t str_len  = read_int32(data + 8);
    if (12 + str_len > len) return;
    auto val = trim_mq_string(std::string(reinterpret_cast<const char*>(data + 12), str_len));

    switch (param_id) {
    case 2016: handle.queue_name = val; break;
    case 3501: handle.channel_name = val; break;
    case 3502: handle.connection_name = val; break;
    case 2549: handle.application_tag = val; break;
    case 2046: handle.user_id = val; break;
    }
}

void PCFInquiry::parse_integer_param(const uint8_t* data, size_t len,
                                     QueueHandleDetails& handle) {
    // data points past Type+StrucLength (at Parameter field)
    // Layout: Parameter(4) + Value(4)
    if (len < 8) return;
    uint32_t param_id = read_int32(data);
    int32_t  value    = static_cast<int32_t>(read_int32(data + 4));

    switch (param_id) {
    case 3002: handle.process_id = value; break;
    case 1411: if (value > 0) handle.input_mode = "INPUT"; break;
    case 1412: if (value > 0) handle.output_mode = "OUTPUT"; break;
    }
}

std::vector<QueueHandleDetails> PCFInquiry::parse_queue_status_response(
        const uint8_t* data, size_t len) {
    std::vector<QueueHandleDetails> handles;
    if (len < MQCFH_SIZE) return handles;

    // CompCode at offset 24
    uint32_t comp_code = read_int32(data + 24);

    if (comp_code != 0) {
        spdlog::warn("PCF response error: comp_code={}, reason={}",
                     comp_code, read_int32(data + 28));
        return handles;
    }

    size_t offset = MQCFH_SIZE; // 36 bytes
    QueueHandleDetails current;
    bool in_group = false;

    while (offset + 8 <= len) {
        uint32_t param_type      = read_int32(data + offset);
        uint32_t param_struc_len = read_int32(data + offset + 4);
        if (param_struc_len == 0 || offset + param_struc_len > len) break;

        const uint8_t* param_data = data + offset + 8;
        size_t param_data_len = param_struc_len - 8;

        switch (param_type) {
        case 20: // MQCFT_GROUP
            if (in_group && !current.queue_name.empty())
                handles.push_back(current);
            current = QueueHandleDetails{};
            in_group = true;
            break;
        case 4: // MQCFT_STRING
            parse_string_param(param_data, param_data_len, current);
            break;
        case 3: // MQCFT_INTEGER
            parse_integer_param(param_data, param_data_len, current);
            break;
        }

        offset += param_struc_len;
    }

    // Push the last parsed entry (works for both grouped and flat responses)
    if (!current.queue_name.empty()) {
        handles.push_back(current);
    }

    spdlog::info("Parsed {} handle details from PCF response", handles.size());
    return handles;
}

} // namespace ibmmq_exporter
