#include "ibmmq_exporter/pcf_parser.h"

#include <algorithm>
#include <cstring>

#include <spdlog/spdlog.h>

namespace ibmmq_exporter {

// Endian helpers — PCF uses little-endian on Windows, big-endian on z/OS.
// The Go code uses LittleEndian for header parsing, so we follow suit.
static int32_t read_le32(const uint8_t* p) {
    return static_cast<int32_t>(
        static_cast<uint32_t>(p[0]) |
        (static_cast<uint32_t>(p[1]) << 8) |
        (static_cast<uint32_t>(p[2]) << 16) |
        (static_cast<uint32_t>(p[3]) << 24));
}

static int64_t read_le64(const uint8_t* p) {
    uint64_t lo = static_cast<uint32_t>(read_le32(p));
    uint64_t hi = static_cast<uint32_t>(read_le32(p + 4));
    return static_cast<int64_t>(lo | (hi << 32));
}

std::string PCFParser::clean_string(const std::string& s) {
    auto result = s;
    auto pos = result.find('\0');
    if (pos != std::string::npos) result.resize(pos);
    pos = result.find_last_not_of(' ');
    if (pos != std::string::npos) result.resize(pos + 1);
    else if (!result.empty() && result.front() == ' ') result.clear();
    return result;
}

PCFHeader PCFParser::parse_header(const uint8_t* data, size_t len) {
    if (len < 36)
        throw std::runtime_error("Insufficient data for PCF header");

    PCFHeader hdr;
    hdr.type            = read_le32(data);
    hdr.struc_length    = read_le32(data + 4);
    hdr.version         = read_le32(data + 8);
    hdr.command         = read_le32(data + 12);
    hdr.msg_seq_number  = read_le32(data + 16);
    hdr.control         = read_le32(data + 20);
    hdr.comp_code       = read_le32(data + 24);
    hdr.reason          = read_le32(data + 28);
    hdr.parameter_count = read_le32(data + 32);
    return hdr;
}

std::vector<PCFParameter> PCFParser::parse_parameters(const uint8_t* data, size_t len) {
    std::vector<PCFParameter> params;
    size_t offset = 0;

    while (offset + 8 <= len) {
        int32_t param_type   = read_le32(data + offset);
        int32_t struc_length = read_le32(data + offset + 4);

        if (struc_length <= 0 || static_cast<size_t>(struc_length) > len - offset)
            break;

        PCFParameter param;
        param.type = param_type;

        switch (param_type) {
        case pcf::MQCFT_INTEGER: {
            if (struc_length >= 16) {
                param.parameter = read_le32(data + offset + 8);
                param.value = read_le32(data + offset + 12);
            }
            break;
        }
        case pcf::MQCFT_INTEGER64: {
            if (struc_length >= 24) {
                param.parameter = read_le32(data + offset + 8);
                param.value = static_cast<int32_t>(read_le64(data + offset + 16));
            }
            break;
        }
        case pcf::MQCFT_STRING: {
            if (struc_length >= 20) {
                param.parameter = read_le32(data + offset + 8);
                int32_t str_len = read_le32(data + offset + 16);
                if (str_len > 0 && offset + 20 + str_len <= len + offset) {
                    std::string s(reinterpret_cast<const char*>(data + offset + 20),
                                  std::min(static_cast<size_t>(str_len),
                                           static_cast<size_t>(struc_length - 20)));
                    param.value = clean_string(s);
                }
            }
            break;
        }
        case pcf::MQCFT_INTEGER_LIST:
        case pcf::MQCFT_INTEGER64_LIST: {
            if (struc_length >= 16) {
                param.parameter = read_le32(data + offset + 8);
                int32_t count = read_le32(data + offset + 12);
                std::vector<int64_t> values;
                size_t val_offset = offset + 16;
                for (int32_t i = 0; i < count && val_offset + 4 <= offset + struc_length; ++i) {
                    if (param_type == pcf::MQCFT_INTEGER64_LIST && val_offset + 8 <= offset + struc_length) {
                        values.push_back(read_le64(data + val_offset));
                        val_offset += 8;
                    } else {
                        values.push_back(read_le32(data + val_offset));
                        val_offset += 4;
                    }
                }
                param.value = std::move(values);
            }
            break;
        }
        case pcf::MQCFT_STRING_LIST: {
            if (struc_length >= 24) {
                param.parameter = read_le32(data + offset + 8);
                int32_t count   = read_le32(data + offset + 16);
                int32_t str_len = read_le32(data + offset + 20);
                std::vector<std::string> strings;
                size_t val_offset = offset + 24;
                for (int32_t i = 0; i < count && val_offset + str_len <= offset + struc_length; ++i) {
                    std::string s(reinterpret_cast<const char*>(data + val_offset), str_len);
                    strings.push_back(clean_string(s));
                    val_offset += str_len;
                }
                param.value = std::move(strings);
            }
            break;
        }
        case pcf::MQCFT_GROUP: {
            if (struc_length >= 16) {
                param.parameter = read_le32(data + offset + 8);
                size_t nested_len = struc_length - 16;
                auto nested = parse_parameters(data + offset + 16, nested_len);
                param.value = std::move(nested);
            }
            break;
        }
        default:
            param.parameter = (struc_length >= 12) ? read_le32(data + offset + 8) : 0;
            break;
        }

        params.push_back(std::move(param));
        offset += struc_length;
    }

    return params;
}

std::optional<ParsedMessage> PCFParser::parse_message(const std::vector<uint8_t>& data,
                                                       const std::string& msg_type) {
    if (data.size() < 36) return std::nullopt;

    auto hdr = parse_header(data.data(), data.size());

    spdlog::debug("PCF message: command={}, type={}, params={}",
                  hdr.command, hdr.type, hdr.parameter_count);

    auto params = parse_parameters(data.data() + 36, data.size() - 36);

    switch (hdr.command) {
    case pcf::CMD_STATISTICS_Q:
    case pcf::CMD_STATISTICS_CHANNEL:
    case pcf::CMD_STATISTICS_MQI:
        return parse_statistics(hdr, params);
    case pcf::CMD_ACCOUNTING_Q:
    case pcf::CMD_ACCOUNTING_MQI:
        return parse_accounting(hdr, params);
    default:
        StatisticsData sd;
        sd.type = msg_type;
        return sd;
    }
}

StatisticsData PCFParser::parse_statistics(const PCFHeader& hdr,
                                            const std::vector<PCFParameter>& params) {
    StatisticsData stats;
    stats.type = "statistics";

    for (const auto& p : params) {
        if (p.parameter == pcf::MQCA_Q_MGR_NAME) {
            if (auto* s = std::get_if<std::string>(&p.value))
                stats.queue_manager = *s;
        }
    }

    switch (hdr.command) {
    case pcf::CMD_STATISTICS_Q:
        stats.queue_stats = parse_queue_stats(params);
        break;
    case pcf::CMD_STATISTICS_CHANNEL:
        stats.channel_stats = parse_channel_stats(params);
        break;
    case pcf::CMD_STATISTICS_MQI:
        stats.mqi_stats = parse_mqi_stats(params);
        break;
    }

    return stats;
}

AccountingData PCFParser::parse_accounting(const PCFHeader& /*hdr*/,
                                            const std::vector<PCFParameter>& params) {
    AccountingData acct;
    acct.type = "accounting";

    for (const auto& p : params) {
        if (p.parameter == pcf::MQCA_Q_MGR_NAME) {
            if (auto* s = std::get_if<std::string>(&p.value))
                acct.queue_manager = *s;
        }
    }

    acct.connection_info = parse_connection_info(params);
    acct.operations = parse_operation_counts(params);

    for (const auto& p : params) {
        if (auto* nested = std::get_if<PCFParameterList>(&p.value)) {
            QueueAppOperation qa;
            for (const auto& np : *nested) {
                if (auto* s = std::get_if<std::string>(&np.value)) {
                    switch (np.parameter) {
                    case pcf::MQCA_Q_NAME:            qa.queue_name = *s; break;
                    case pcf::MQCA_APPL_NAME:
                    case pcf::MQCACF_APPL_NAME:       qa.application_name = *s; break;
                    case pcf::MQCA_CONNECTION_NAME:
                    case pcf::MQCACH_CONNECTION_NAME:  qa.connection_name = *s; break;
                    case pcf::MQCACF_USER_IDENTIFIER:  qa.user_identifier = *s; break;
                    }
                }
                if (auto* v = std::get_if<int32_t>(&np.value)) {
                    switch (np.parameter) {
                    case pcf::MQIAMO_PUTS:          qa.puts = *v; break;
                    case pcf::MQIAMO_GETS:          qa.gets = *v; break;
                    case pcf::MQIAMO_MSGS_RCVD: qa.msgs_received = *v; break;
                    case pcf::MQIAMO_MSGS_SENT:     qa.msgs_sent = *v; break;
                    }
                }
            }
            if (!qa.queue_name.empty() &&
                (qa.puts || qa.gets || qa.msgs_received || qa.msgs_sent)) {
                acct.queue_operations.push_back(std::move(qa));
            }
        }
    }

    return acct;
}

QueueStatistics PCFParser::parse_queue_stats(const std::vector<PCFParameter>& params) {
    QueueStatistics stats;

    for (const auto& p : params) {
        if (auto* nested = std::get_if<PCFParameterList>(&p.value)) {
            ProcInfo proc;
            proc.role = "unknown";
            for (const auto& np : *nested) {
                if (auto* s = std::get_if<std::string>(&np.value)) {
                    switch (np.parameter) {
                    case pcf::MQCA_APPL_NAME:
                    case pcf::MQCACF_APPL_NAME:       proc.application_name = *s; break;
                    case pcf::MQCA_CONNECTION_NAME:
                    case pcf::MQCACH_CONNECTION_NAME:  proc.connection_name = *s; break;
                    case pcf::MQCACF_USER_IDENTIFIER:  proc.user_identifier = *s; break;
                    case pcf::MQCA_CHANNEL_NAME:       proc.channel_name = *s; break;
                    case pcf::MQCACF_APPL_TAG:         proc.application_tag = *s; break;
                    }
                }
                if (auto* v = std::get_if<int32_t>(&np.value)) {
                    switch (np.parameter) {
                    case pcf::MQIA_OPEN_INPUT_COUNT:
                        if (*v > 0) proc.role = "input";
                        break;
                    case pcf::MQIA_OPEN_OUTPUT_COUNT:
                        if (*v > 0) proc.role = "output";
                        break;
                    case pcf::MQIACF_PROCESS_ID:
                        proc.process_id = *v; break;
                    }
                }
            }
            if (!proc.application_name.empty() || !proc.connection_name.empty() ||
                !proc.user_identifier.empty()) {
                stats.associated_procs.push_back(std::move(proc));
            }
            continue;
        }

        if (auto* v = std::get_if<int32_t>(&p.value)) {
            switch (p.parameter) {
            case pcf::MQIA_CURRENT_Q_DEPTH:   stats.current_depth = *v; break;
            case pcf::MQIA_HIGH_Q_DEPTH:      stats.high_depth = *v; break;
            case pcf::MQIA_OPEN_INPUT_COUNT:
                stats.input_count = *v;
                stats.has_readers = (*v > 0);
                break;
            case pcf::MQIA_OPEN_OUTPUT_COUNT:
                stats.output_count = *v;
                stats.has_writers = (*v > 0);
                break;
            case pcf::MQIA_MSG_ENQ_COUNT:     stats.enqueue_count = *v; break;
            case pcf::MQIA_MSG_DEQ_COUNT:     stats.dequeue_count = *v; break;
            }
        } else if (auto* s = std::get_if<std::string>(&p.value)) {
            if (p.parameter == pcf::MQCA_Q_NAME) stats.queue_name = *s;
        }
    }

    return stats;
}

ChannelStatistics PCFParser::parse_channel_stats(const std::vector<PCFParameter>& params) {
    ChannelStatistics stats;

    for (const auto& p : params) {
        if (auto* v = std::get_if<int32_t>(&p.value)) {
            switch (p.parameter) {
            case pcf::MQIACH_MSGS:    stats.messages = *v; break;
            case pcf::MQIACH_BYTES:   stats.bytes = *v; break;
            case pcf::MQIACH_BATCHES: stats.batches = *v; break;
            }
        } else if (auto* s = std::get_if<std::string>(&p.value)) {
            switch (p.parameter) {
            case pcf::MQCA_CHANNEL_NAME:    stats.channel_name = *s; break;
            case pcf::MQCA_CONNECTION_NAME: stats.connection_name = *s; break;
            }
        }
    }

    return stats;
}

MQIStatistics PCFParser::parse_mqi_stats(const std::vector<PCFParameter>& params) {
    MQIStatistics stats;

    for (const auto& p : params) {
        if (auto* nested = std::get_if<PCFParameterList>(&p.value)) {
            auto ns = parse_mqi_stats(*nested);
            stats.opens += ns.opens; stats.closes += ns.closes;
            stats.puts += ns.puts; stats.gets += ns.gets;
            stats.commits += ns.commits; stats.backouts += ns.backouts;
            if (!ns.application_name.empty()) stats.application_name = ns.application_name;
            continue;
        }

        if (auto* vlist = std::get_if<std::vector<int64_t>>(&p.value)) {
            int32_t sum32 = 0; int64_t sum64 = 0;
            for (auto v : *vlist) { sum32 += static_cast<int32_t>(v); sum64 += v; }

            switch (p.parameter) {
            case pcf::MQIAMO_OPENS:  stats.opens = sum32; break;
            case pcf::MQIAMO_CLOSES: stats.closes = sum32; break;
            case pcf::MQIAMO_PUTS:   stats.puts = sum32; break;
            case pcf::MQIAMO_GETS:   stats.gets = sum32; break;
            case pcf::MQIAMO_COMMITS: stats.commits = sum32; break;
            case pcf::MQIAMO_BACKOUTS: stats.backouts = sum32; break;
            case pcf::MQIAMO_BROWSES: stats.browses = sum32; break;
            case pcf::MQIAMO_INQS:    stats.inqs = sum32; break;
            case pcf::MQIAMO_SETS:    stats.sets = sum32; break;
            case pcf::MQIAMO_MSGS_RCVD: stats.msgs_received = sum32; break;
            case pcf::MQIAMO_MSGS_SENT:     stats.msgs_sent = sum32; break;
            case pcf::MQIAMO_MSG_BYTES_RCVD: stats.bytes_received = sum64; break;
            case pcf::MQIAMO_BYTES_SENT:     stats.bytes_sent = sum64; break;
            case pcf::MQIAMO_INCOMPLETE_BATCHES: stats.incomplete_batch = sum32; break;
            case pcf::MQIAMO_SYNCPOINT_HEURISTIC: stats.syncpoint_heuristic = sum32; break;
            case pcf::MQIAMO_WAIT_INTERVAL: stats.wait_interval = sum32; break;
            case pcf::MQIAMO_HEAPS:         stats.heaps = sum32; break;
            case pcf::MQIAMO_LOGICAL_CONNECTIONS: stats.logical_connections = sum32; break;
            case pcf::MQIAMO_PHYSICAL_CONNECTIONS: stats.physical_connections = sum32; break;
            case pcf::MQIAMO_CURRENT_CONNS: stats.current_conns = sum32; break;
            case pcf::MQIAMO_PERSISTENT_MSGS: stats.persistent_msgs = sum32; break;
            case pcf::MQIAMO_NON_PERSISTENT_MSGS: stats.non_persistent_msgs = sum32; break;
            case pcf::MQIAMO_LONG_MSGS:  stats.long_msgs = sum32; break;
            case pcf::MQIAMO_SHORT_MSGS: stats.short_msgs = sum32; break;
            case pcf::MQIAMO_QUEUE_TIME:     stats.queue_time = sum64; break;
            case pcf::MQIAMO_QUEUE_TIME_MAX: stats.queue_time_max = sum64; break;
            case pcf::MQIAMO_ELAPSED_TIME:   stats.elapsed_time = sum64; break;
            case pcf::MQIAMO_ELAPSED_TIME_MAX: stats.elapsed_time_max = sum64; break;
            case pcf::MQIAMO_CONN_TIME:      stats.conn_time = sum64; break;
            case pcf::MQIAMO_CONN_TIME_MAX:  stats.conn_time_max = sum64; break;
            }
            continue;
        }

        if (auto* v = std::get_if<int32_t>(&p.value)) {
            switch (p.parameter) {
            case pcf::MQIAMO_OPENS:  stats.opens = *v; break;
            case pcf::MQIAMO_CLOSES: stats.closes = *v; break;
            case pcf::MQIAMO_PUTS:   stats.puts = *v; break;
            case pcf::MQIAMO_GETS:   stats.gets = *v; break;
            case pcf::MQIAMO_COMMITS: stats.commits = *v; break;
            case pcf::MQIAMO_BACKOUTS: stats.backouts = *v; break;
            case pcf::MQIAMO_BROWSES: stats.browses = *v; break;
            case pcf::MQIAMO_INQS:    stats.inqs = *v; break;
            case pcf::MQIAMO_SETS:    stats.sets = *v; break;
            case pcf::MQIAMO_DISC_CLOSE_TIMEOUT: stats.disc_close_timeout = *v; break;
            case pcf::MQIAMO_DISC_RESET_TIMEOUT: stats.disc_reset_timeout = *v; break;
            case pcf::MQIAMO_FAILS:          stats.fails = *v; break;
            case pcf::MQIAMO_INCOMPLETE_BATCHES: stats.incomplete_batch = *v; break;
            case pcf::MQIAMO_INCOMPLETE_MSG:  stats.incomplete_msg = *v; break;
            case pcf::MQIAMO_WAIT_INTERVAL:  stats.wait_interval = *v; break;
            case pcf::MQIAMO_SYNCPOINT_HEURISTIC: stats.syncpoint_heuristic = *v; break;
            case pcf::MQIAMO_HEAPS:          stats.heaps = *v; break;
            case pcf::MQIAMO_LOGICAL_CONNECTIONS: stats.logical_connections = *v; break;
            case pcf::MQIAMO_PHYSICAL_CONNECTIONS: stats.physical_connections = *v; break;
            case pcf::MQIAMO_CURRENT_CONNS:  stats.current_conns = *v; break;
            case pcf::MQIAMO_PERSISTENT_MSGS: stats.persistent_msgs = *v; break;
            case pcf::MQIAMO_NON_PERSISTENT_MSGS: stats.non_persistent_msgs = *v; break;
            case pcf::MQIAMO_LONG_MSGS:  stats.long_msgs = *v; break;
            case pcf::MQIAMO_SHORT_MSGS: stats.short_msgs = *v; break;
            case pcf::MQIAMO_STAMP_ENABLED: stats.stamp_enabled = *v; break;
            case pcf::MQIAMO_MSGS_RCVD: stats.msgs_received = *v; break;
            case pcf::MQIAMO_MSGS_SENT:     stats.msgs_sent = *v; break;
            case pcf::MQIACH_CHANNEL_STATUS: stats.channel_status = *v; break;
            case pcf::MQIACH_CHANNEL_TYPE:   stats.channel_type = *v; break;
            // channel_errors, channel_disc_count, channel_exit_name have no
            // real IBM MQ parameter IDs — they are not emitted in PCF messages.
            case pcf::MQIAMO_FULL_BATCHES:  stats.full_batches = *v; break;
            }
        } else if (auto* s = std::get_if<std::string>(&p.value)) {
            switch (p.parameter) {
            case pcf::MQCA_APPL_NAME:
            case pcf::MQCACF_APPL_NAME:       stats.application_name = *s; break;
            case pcf::MQCACF_APPL_TAG:         stats.application_tag = *s; break;
            case pcf::MQCACF_USER_IDENTIFIER:  stats.user_identifier = *s; break;
            case pcf::MQCACH_CONNECTION_NAME:   stats.connection_name = *s; break;
            case pcf::MQCA_CHANNEL_NAME:        stats.channel_name = *s; break;
            }
        }
    }

    return stats;
}

ConnectionInfo PCFParser::parse_connection_info(const std::vector<PCFParameter>& params) {
    ConnectionInfo info;
    for (const auto& p : params) {
        if (auto* s = std::get_if<std::string>(&p.value)) {
            switch (p.parameter) {
            case pcf::MQCA_CHANNEL_NAME:       info.channel_name = *s; break;
            case pcf::MQCA_CONNECTION_NAME:
            case pcf::MQCACH_CONNECTION_NAME:   info.connection_name = *s; break;
            case pcf::MQCA_APPL_NAME:
            case pcf::MQCACF_APPL_NAME:        info.application_name = *s; break;
            case pcf::MQCACF_APPL_TAG:          info.application_tag = *s; break;
            case pcf::MQCACF_USER_IDENTIFIER:   info.user_identifier = *s; break;
            }
        }
    }
    return info;
}

OperationCounts PCFParser::parse_operation_counts(const std::vector<PCFParameter>& params) {
    OperationCounts ops;
    for (const auto& p : params) {
        if (auto* v = std::get_if<int32_t>(&p.value)) {
            switch (p.parameter) {
            case pcf::MQIAMO_GETS:     ops.gets = *v; break;
            case pcf::MQIAMO_PUTS:     ops.puts = *v; break;
            case pcf::MQIAMO_OPENS:    ops.opens = *v; break;
            case pcf::MQIAMO_CLOSES:   ops.closes = *v; break;
            case pcf::MQIAMO_COMMITS:  ops.commits = *v; break;
            case pcf::MQIAMO_BACKOUTS: ops.backouts = *v; break;
            }
        }
    }
    return ops;
}

} // namespace ibmmq_exporter
