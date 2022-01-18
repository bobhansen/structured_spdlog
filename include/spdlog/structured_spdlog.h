// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

// spdlog main header file.
// see example.cpp for usage example

#ifndef STRUCTURED_SPDLOG_H
#define STRUCTURED_SPDLOG_H

#pragma once

#include "spdlog.h"

namespace spdlog {
namespace details {
    void append_value(const Field &field, memory_buf_t &dest);
    std::string value_to_string(const Field &field);
}

inline void log(source_loc source, level::level_enum lvl, std::initializer_list<Field> fields, string_view_t msg)
{
    default_logger_raw()->log(source, lvl, fields, msg);
}


inline void log(level::level_enum lvl, std::initializer_list<Field> fields, string_view_t msg)
{
    default_logger_raw()->log(source_loc{}, lvl, fields, msg);
}


inline void trace(std::initializer_list<Field> fields, string_view_t msg)
{
    default_logger_raw()->log(source_loc{}, level::trace, fields, msg);
}


inline void debug(std::initializer_list<Field> fields, string_view_t msg)
{
    default_logger_raw()->log(source_loc{}, level::debug, fields, msg);
}


inline void info(std::initializer_list<Field> fields, string_view_t msg)
{
    default_logger_raw()->log(source_loc{}, level::info, fields, msg);
}


inline void warn(std::initializer_list<Field> fields, string_view_t msg)
{
    default_logger_raw()->log(source_loc{}, level::warn, fields, msg);
}


inline void error(std::initializer_list<Field> fields, string_view_t msg)
{
    default_logger_raw()->log(source_loc{}, level::err, fields, msg);
}


inline void critical(std::initializer_list<Field> fields, string_view_t msg)
{
    default_logger_raw()->log(source_loc{}, level::critical, fields, msg);
}


}

#endif // STRUCTURED_SPDLOG_H

#ifdef SPDLOG_HEADER_ONLY
#    include "structured_spdlog-inl.h"
#endif
