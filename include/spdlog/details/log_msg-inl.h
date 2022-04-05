// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#ifndef SPDLOG_HEADER_ONLY
#    include <spdlog/details/log_msg.h>
#endif

#include <spdlog/details/os.h>

namespace spdlog {
namespace details {

SPDLOG_INLINE log_msg::log_msg(spdlog::log_clock::time_point log_time, spdlog::source_loc loc, string_view_t a_logger_name,
    spdlog::level::level_enum lvl, spdlog::string_view_t msg, const Field * fields, size_t field_count)
    : logger_name(a_logger_name)
    , level(lvl)
    , time(log_time)
#ifndef SPDLOG_NO_THREAD_ID
    , thread_id(os::thread_id())
#endif
    , source(loc)
    , payload(msg)
#ifndef SPDLOG_NO_STRUCTURED_SPDLOG
    , field_data(const_cast<Field *>(fields))
    , field_data_count(field_count)
    , context_field_data(threadlocal_context_head())
#endif
{
#ifdef SPDLOG_NO_STRUCTURED_SPDLOG
    // Prevent unused argument warning
    (void) fields;
    (void) field_count;
#endif
}

SPDLOG_INLINE log_msg::log_msg(spdlog::log_clock::time_point log_time, spdlog::source_loc loc, string_view_t a_logger_name,
    spdlog::level::level_enum lvl, spdlog::string_view_t msg)
    : log_msg(log_time, loc, a_logger_name, lvl, msg, nullptr, 0)
{}

SPDLOG_INLINE log_msg::log_msg(
    spdlog::source_loc loc, string_view_t a_logger_name, spdlog::level::level_enum lvl, spdlog::string_view_t msg)
    : log_msg(os::now(), loc, a_logger_name, lvl, msg, nullptr, 0)
{}

SPDLOG_INLINE log_msg::log_msg(
    spdlog::source_loc loc, string_view_t a_logger_name, spdlog::level::level_enum lvl, spdlog::string_view_t msg,
    const Field * fields, size_t field_count)
    : log_msg(os::now(), loc, a_logger_name, lvl, msg, fields, field_count)
{}

SPDLOG_INLINE log_msg::log_msg(string_view_t a_logger_name, spdlog::level::level_enum lvl, spdlog::string_view_t msg)
    : log_msg(os::now(), source_loc{}, a_logger_name, lvl, msg, nullptr, 0)
{}

} // namespace details
} // namespace spdlog
