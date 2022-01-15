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

    // Stop on empty arg list
    template<size_t N>
    void fill_fields(std::array<Field, N> & result [[maybe_unused]], int n [[maybe_unused]]) {
    }

    template<typename... ArgTypes, size_t N>
    void fill_fields(std::array<Field, N> & result, int n, const Field& f, ArgTypes... args) {
        result[n] = f;
        fill_fields(result, n+1, std::forward<ArgTypes>(args)...);
    }

    template<typename... ArgTypes>
    std::array<Field, sizeof...(ArgTypes)> build_fields(ArgTypes... args) {
        std::array<Field, sizeof...(ArgTypes)> result;
        fill_fields(result, 0, std::forward<ArgTypes>(args)...);
        return result;
    }


    template<typename... Args, size_t N>
    inline void slog(source_loc source, level::level_enum lvl, const std::array<Field,N>& fields, format_string_t<Args...> fmt, Args &&... args)
    {
        default_logger_raw()->slog(source, lvl, fields, fmt, std::forward<Args>(args)...);
    }


}

#endif // STRUCTURED_SPDLOG_H

#ifdef SPDLOG_HEADER_ONLY
#    include "structured_spdlog-inl.h"
#endif
