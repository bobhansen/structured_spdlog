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

constexpr char ISO8601_FLAGS[] = "%Y-%m-%dT%H:%M:%S.%f%z";

enum class json_field_type {NUMERIC, STRING};
namespace details {
    void escape_to_end(spdlog::memory_buf_t &dest, size_t start_offset);
    bool pattern_needs_escaping(string_view_t pattern);

    class pattern_field {
    public:
        pattern_field(const std::string &name, const std::string &pattern, json_field_type field_type, pattern_time_type pattern_time_type_);
        pattern_field(const std::string &name, formatter* formatter, json_field_type field_type, bool output_needs_escaping);

        pattern_field(const pattern_field &other) = delete;
        pattern_field &operator=(const pattern_field &other) = delete;

        void format(const details::log_msg &msg, memory_buf_t &dest);

        std::unique_ptr<pattern_field> clone();
    private:
        std::string value_prefix_; // {"name":}
        std::unique_ptr<formatter> formatter_;
        json_field_type field_type_;
        bool output_needs_escaping_;
    };
} // namespace details

class SPDLOG_API json_formatter final : public formatter
{
public:

    // Default construction
    explicit json_formatter(pattern_time_type time_type = pattern_time_type::local, std::string eol = spdlog::details::os::default_eol);
    json_formatter &add_field(std::string field_name, std::string pattern, json_field_type field_type = json_field_type::STRING);
    json_formatter &add_default_fields();


    json_formatter(const json_formatter &other) = delete;
    json_formatter &operator=(const json_formatter &other) = delete;

    std::unique_ptr<formatter> clone() const override;
    void format(const details::log_msg &msg, memory_buf_t &dest) override;

private:
    void format_data_fields(const Field * fields, size_t field_count, spdlog::memory_buf_t &dest);

    pattern_time_type pattern_time_type_;
    std::string eol_;

    std::vector<std::unique_ptr<details::pattern_field>> fields_;
};


} // namespace spdlog

#ifdef SPDLOG_HEADER_ONLY
#    include "json_formatter-inl.h"
#endif
