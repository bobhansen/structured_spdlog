// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include <spdlog/common.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/details/os.h>
#include <spdlog/formatter.h>
#include <spdlog/pattern_formatter.h>

#include <chrono>
#include <ctime>
#include <memory>

#include <string>
#include <vector>
#include <unordered_map>

namespace spdlog {

class SPDLOG_API json_formatter final : public formatter
{
public:
    // Default construction
    explicit json_formatter(pattern_time_type time_type = pattern_time_type::local, std::string eol = spdlog::details::os::default_eol);

    // To support cloning; note that it takes default time_type and eol
    explicit json_formatter(
        std::string escaped_time_field_name,
        std::string escaped_message_field_name,
        std::string escaped_source_loc_field_name,
        std::string escaped_level_field_name,
        std::string escaped_thread_id_field_name
        );

    // Set field name to "" to not emit the field
    json_formatter & set_time_field(string_view_t time_field_name, pattern_time_type);
    json_formatter & set_message_field(string_view_t message_field_name);
    json_formatter & set_source_loc_field(string_view_t source_loc_field_name);
    json_formatter & set_level_field(string_view_t level_field_name);
    json_formatter & set_thread_id_field(string_view_t thread_id_field_name);

    json_formatter(const json_formatter &other) = delete;
    json_formatter &operator=(const json_formatter &other) = delete;

    std::unique_ptr<formatter> clone() const override;
    void format(const details::log_msg &msg, memory_buf_t &dest) override;

private:
    void compile_pattern();

    pattern_time_type pattern_time_type_;
    std::string escaped_time_field_name_;
    std::string escaped_message_field_name_;
    std::string escaped_source_loc_field_name_;
    std::string escaped_level_field_name_;
    std::string escaped_thread_id_field_name_;

    spdlog::pattern_formatter internal_formatter_;
};

namespace details {
    void escape_to_end(spdlog::memory_buf_t &dest, size_t start_offset);
} // namespace details

} // namespace spdlog

#ifdef SPDLOG_HEADER_ONLY
#    include "json_formatter-inl.h"
#endif
