// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#ifndef SPDLOG_HEADER_ONLY
#    include <spdlog/details/log_msg_buffer.h>
#endif

#include <iostream>

namespace spdlog {
namespace details {

SPDLOG_INLINE log_msg_buffer::log_msg_buffer(const log_msg &orig_msg)
    : log_msg{orig_msg}
{
#ifndef SPDLOG_NO_STRUCTURED_SPDLOG
    // Start with struct copies so that objects stay nicely aligned, but strings
    //   can be packed in at the end
    buffer.append(reinterpret_cast<const char*>(field_data), reinterpret_cast<const char*>(field_data + field_data_count));
    for (size_t i=0; i < field_data_count; i++) {
        buffer.append(field_data[i].name);
        if (field_data[i].value_type == FieldValueType::STRING_VIEW) {
            buffer.append(field_data[i].string_view_.begin(), field_data[i].string_view_.end());
        }
    }
#endif
    buffer.append(logger_name.begin(), logger_name.end());
    buffer.append(payload.begin(), payload.end());
    update_string_views();
}

SPDLOG_INLINE log_msg_buffer::log_msg_buffer(const log_msg_buffer &other)
    : log_msg{other}
{
#ifndef SPDLOG_NO_STRUCTURED_SPDLOG
    // Start with struct copies so that objects stay nicely aligned, but strings
    //   can be packed in at the end
    buffer.append(reinterpret_cast<const char*>(field_data), reinterpret_cast<const char*>(field_data + field_data_count));
    for (size_t i=0; i < field_data_count; i++) {
        buffer.append(field_data[i].name);
        if (field_data[i].value_type == FieldValueType::STRING_VIEW) {
            buffer.append(field_data[i].string_view_.begin(), field_data[i].string_view_.end());
        }
    }
#endif
    buffer.append(logger_name.begin(), logger_name.end());
    buffer.append(payload.begin(), payload.end());
    update_string_views();
}

SPDLOG_INLINE log_msg_buffer::log_msg_buffer(log_msg_buffer &&other) SPDLOG_NOEXCEPT : log_msg{other}, buffer{std::move(other.buffer)}
{
    std::cout << "this_count=" << field_data_count << " other_count=" << other.field_data_count << std::endl;
    update_string_views();
}

SPDLOG_INLINE log_msg_buffer &log_msg_buffer::operator=(const log_msg_buffer &other)
{
    log_msg::operator=(other);
    buffer.clear();
    buffer.append(other.buffer.data(), other.buffer.data() + other.buffer.size());
    update_string_views();
    return *this;
}

SPDLOG_INLINE log_msg_buffer &log_msg_buffer::operator=(log_msg_buffer &&other) SPDLOG_NOEXCEPT
{
    log_msg::operator=(other);
    buffer = std::move(other.buffer);
    if (field_data_count != other.field_data_count) {
        abort();
    }
    update_string_views();
    return *this;
}

SPDLOG_INLINE void log_msg_buffer::update_string_views()
{
#ifndef SPDLOG_NO_STRUCTURED_SPDLOG
    field_data = reinterpret_cast<Field *>(buffer.data());
    size_t offset = sizeof(Field) * field_data_count;
    for (size_t i=0; i < field_data_count; i++) {
        field_data[i].name = string_view_t{buffer.data() + offset, field_data[i].name.size()};
        offset += field_data[i].name.size();
        if (field_data[i].value_type == FieldValueType::STRING_VIEW) {
            auto &data_value = field_data[i].string_view_;
            data_value = {buffer.data() + offset, data_value.size()};
            offset += data_value.size();
        }
    }
#endif
    logger_name = string_view_t{buffer.data() + offset, logger_name.size()};
    offset += logger_name.size();
    payload = string_view_t{buffer.data() + offset, payload.size()};
    offset += payload.size();
}

} // namespace details
} // namespace spdlog
