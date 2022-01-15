// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#ifndef STRUCTURED_SPDLOG_INL_H
#define STRUCTURED_SPDLOG_INL_H

#pragma once

#ifndef SPDLOG_HEADER_ONLY
#    include <spdlog/structured_spdlog.h>
#endif

namespace spdlog {
namespace details {

SPDLOG_INLINE void append_value(const Field &field, memory_buf_t &dest)
{
    switch(field.value_type) {
        case FieldValueType::STRING_VIEW: {
            auto *buf_ptr = field.string_view_.data();
            dest.append(buf_ptr, buf_ptr + field.string_view_.size());
            break;
        }
        case FieldValueType::SHORT:     details::fmt_helper::append_int(field.short_,     dest); break;
        case FieldValueType::USHORT:    details::fmt_helper::append_int(field.ushort_,    dest); break;
        case FieldValueType::INT:       details::fmt_helper::append_int(field.int_,       dest); break;
        case FieldValueType::UINT:      details::fmt_helper::append_int(field.uint_,      dest); break;
        case FieldValueType::LONG:      details::fmt_helper::append_int(field.long_,      dest); break;
        case FieldValueType::ULONG:     details::fmt_helper::append_int(field.ulong_,     dest); break;
        case FieldValueType::LONGLONG:  details::fmt_helper::append_int(field.longlong_,  dest); break;
        case FieldValueType::ULONGLONG: details::fmt_helper::append_int(field.ulonglong_, dest); break;

        case FieldValueType::BOOL:      details::fmt_helper::append_string_view(field.bool_?"true":"false", dest); break;

        case FieldValueType::CHAR:      dest.push_back(field.char_); break;
        case FieldValueType::UCHAR:     details::fmt_helper::append_int(field.uchar_, dest); break;
        case FieldValueType::WCHAR:     details::fmt_helper::append_string_view(std::to_string(field.wchar_),       dest); break;

        // TODO: optimize these to only have at most one reallocation
        case FieldValueType::FLOAT:      details::fmt_helper::append_string_view(std::to_string(field.float_),      dest); break;
        case FieldValueType::DOUBLE:     details::fmt_helper::append_string_view(std::to_string(field.double_),     dest); break;
        case FieldValueType::LONGDOUBLE: details::fmt_helper::append_string_view(std::to_string(field.longdouble_), dest); break;
    }
}

SPDLOG_INLINE std::string value_to_string(const Field &field)
{
    memory_buf_t buf;
    append_value(field, buf);
    return to_string(buf);
}

} // namespace details

} // namespace spdlog

#endif
