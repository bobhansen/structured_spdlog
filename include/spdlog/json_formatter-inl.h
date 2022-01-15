// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#ifndef SPDLOG_HEADER_ONLY
#    include <spdlog/json_formatter.h>
#endif

#include <spdlog/details/fmt_helper.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/formatter.h>
#include <spdlog/structured_spdlog.h>

#include <chrono>
#include <ctime>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace spdlog {

namespace details {

constexpr char hex_digits[] = "0123456789abcdef";

// 5 -> escape to \uXXXXX
// 1 -> escape to \n, \r or the like
constexpr uint8_t extra_chars_lookup[256] = {
    5, 5, 5, 5, 5, 5, 5, 5, 1, 1, 1, 5, 1, 1, 5, 5, // 0x0x
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, // 0x1x
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x2x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x3x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x4x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, // 0x5x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x6x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x7x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x8x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x9x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xAx
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xBx
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xCx
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xDx
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xEx
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xFx
};

// TODO: make a version that can take a list of [start,end] ranges; we can then
//    write the whole line to the dest buffer and do one pass of escaping: count,
//    realloc, escape

void escape_to_end(spdlog::memory_buf_t &dest, size_t start_offset)
{
   // Check to see if we have any characters that must be escaped
   //   See https://datatracker.ietf.org/doc/html/rfc8259#section-7
   // Note that only certain ASCII characters need to be; all non-ASCII
   //   utf-8 codepoints can pass right through

   // TODO: Note that this code is not correct for UTF-8 inputs;  if we have
   //   a UTF-8 value like \u03a3 (Sigma), the UTF-8 encoding is \xCE\xA3, and
   //   it will be encoded as \u00ce\u00a3 rather than \u03a3.
   //
   //   We need to actually parse the input as utf-8 codepoints to
   //   know which bytes are consumed in multi-byte utf-8 encodings
   //   See https://datatracker.ietf.org/doc/html/rfc3629

   // TODO: widechar support
   static_assert(sizeof(dest[0]) ==1, "Wide chars are not supported by escape_to_end yet");

   size_t extra_chars_required = 0;
   for (auto i=start_offset; i < dest.size(); i++) {
       uint8_t c = dest[i]; // need to make it unsigned
       extra_chars_required += extra_chars_lookup[c];
   }
   if (extra_chars_required == 0) {
       return; // No escaping to be done
   }
   size_t original_size = dest.size();
   dest.resize(dest.size() + extra_chars_required);

   // Work backward until done
   auto start_p = dest.data() + start_offset;
   auto src_p = dest.data() + original_size - 1;
   auto dest_p = src_p + extra_chars_required;
   while (src_p >= start_p) {
       uint8_t c = *src_p;
       switch(extra_chars_lookup[c]) {
          case 5:
            dest_p -= 5;
            dest_p[0] = '\\';
            dest_p[1] = 'u';
            dest_p[2] = '0';
            dest_p[3] = '0';
            dest_p[4] = hex_digits[(c >> 4) & 0x0f];
            dest_p[5] = hex_digits[c & 0x0f];
            break;
        case 1:
           dest_p -= 1;
           dest_p[0] = '\\';
           switch(c) {
               case '"':
                  dest_p[1] = '"';
                  break;
               case '\\':
                  dest_p[1] = '\\';
                  break;
               case '\b':
                  dest_p[1] = 'b';
                  break;
               case '\f':
                  dest_p[1] = 'f';
                  break;
               case '\n':
                  dest_p[1] = 'n';
                  break;
               case '\r':
                  dest_p[1] = 'r';
                  break;
               case '\t':
                  dest_p[1] = 't';
                  break;
                default:
                  abort(); // should never get here
           } // switch(c)
           break;
        case 0:
           *dest_p = c;
           break;
        default:
            abort(); // should never get here
       } // switch(extra_chars_lookup[c])

       dest_p--;
       src_p--;
   }

   // They should have converged at the start
   assert(dest_p == src_p);
}

SPDLOG_INLINE bool pattern_needs_escaping(string_view_t pattern)
{
    // Look for any % pattern that isn't in our whitelist of known-doesn't-need-escaping patterns
    // Anything that can only emit printable ASCII is OK.

    constexpr char KNOWN_CLEAN_PATTERNS[] = "LtplLaAbBcCYDxmdHIMSefFprRTXzE%#oiuO";

    for (size_t i=0; i < pattern.size(); i++) {
        // Check for % flags
        uint8_t c = static_cast<uint8_t>(pattern[i]);
        if (c == '%' && i < pattern.size() - 1) {
            i++;
            c = static_cast<uint8_t>(pattern[i]);

            // TODO: make this a binary search
            bool flag_char_is_clean = false;
            for (auto clean: KNOWN_CLEAN_PATTERNS) {
                if (clean == c) {
                    flag_char_is_clean = true;
                    break;
                }
            }
            if (!flag_char_is_clean) {
                return true;
            }
        }

        // Check for json-escaped character in pattern
        if (extra_chars_lookup[c] != 0) {
            return true;
        }
    }
    return false;
}

SPDLOG_INLINE pattern_field::pattern_field(const std::string &name, const std::string &pattern, json_field_type field_type, pattern_time_type pattern_time_type_) :
    formatter_(details::make_unique<pattern_formatter>(pattern, pattern_time_type_, "")),
    field_type_(field_type)
{
    // TODO - make this all one pattern {"name": "%v", }; since we know the prefix and suffix length,
    //    we can do one call to format(), then escape just the questionable data
    memory_buf_t value_prefix;
    value_prefix.push_back('"');
    fmt_helper::append_string_view(name, value_prefix);
    escape_to_end(value_prefix, 1);
    value_prefix.push_back('"');
    value_prefix.push_back(':');
    value_prefix_ = to_string(value_prefix);

    output_needs_escaping_ = pattern_needs_escaping(pattern);
}

SPDLOG_INLINE pattern_field::pattern_field(const std::string &value_prefix, formatter* formatter, json_field_type field_type, bool output_needs_escaping) :
    value_prefix_(value_prefix), formatter_(std::move(formatter->clone())), field_type_(field_type), output_needs_escaping_(output_needs_escaping)
{
}

SPDLOG_INLINE std::unique_ptr<pattern_field> pattern_field::clone()
{
    return make_unique<pattern_field>(value_prefix_, formatter_.get(), field_type_, output_needs_escaping_);
}

SPDLOG_INLINE void pattern_field::format(const details::log_msg &msg, memory_buf_t &dest)
{
    fmt_helper::append_string_view(value_prefix_, dest);
    if (field_type_ == json_field_type::STRING) {
        dest.push_back('"');
    }
    size_t start_offset = dest.size();
    formatter_->format(msg, dest);
    if (output_needs_escaping_) {
        escape_to_end(dest, start_offset);
    }
    if (field_type_ == json_field_type::STRING) {
        fmt_helper::append_string_view("\", ", dest);
    } else {
        fmt_helper::append_string_view(", ", dest);
    }
}

} // namespace details


SPDLOG_INLINE json_formatter::json_formatter(pattern_time_type time_type, std::string eol) :
    pattern_time_type_(time_type),
    eol_(eol)
{
}

SPDLOG_INLINE json_formatter& json_formatter::add_default_fields()
{
    return add_field("time", ISO8601_FLAGS).add_field("level", "%l").
        add_field("msg", "%v", json_field_type::STRING).add_field("src_loc", "%@");
}

SPDLOG_INLINE json_formatter &json_formatter::add_field(std::string field_name, std::string pattern, json_field_type field_type)
{
    fields_.emplace_back(
        details::make_unique<details::pattern_field>(field_name, pattern, field_type, pattern_time_type_)
    );
    return *this;
}


SPDLOG_INLINE std::unique_ptr<formatter> json_formatter::clone() const
{
    auto result = details::make_unique<json_formatter>(pattern_time_type_, eol_);
    for (auto &field: fields_) {
        result->fields_.emplace_back(std::move(field->clone()));
    }
    return result;
}

SPDLOG_INLINE bool is_numeric(FieldValueType type)
{
    switch(type) {
        case FieldValueType::STRING_VIEW: return false;
        case FieldValueType::SHORT: return true;
        case FieldValueType::USHORT:  return true;
        case FieldValueType::INT: return true;
        case FieldValueType::UINT: return true;
        case FieldValueType::LONG: return true;
        case FieldValueType::ULONG: return true;
        case FieldValueType::LONGLONG: return true;
        case FieldValueType::ULONGLONG: return true;
        case FieldValueType::BOOL: return true; // in JSON, it is
        case FieldValueType::CHAR: return false;
        case FieldValueType::UCHAR: return true;
        case FieldValueType::WCHAR: return false;
        case FieldValueType::FLOAT: return true;
        case FieldValueType::DOUBLE: return true;
        case FieldValueType::LONGDOUBLE: return true;
    }
    abort();  // we should never get here
}

SPDLOG_INLINE void json_formatter::format_data_fields(const Field * fields, size_t field_count, spdlog::memory_buf_t &dest)
{
    // TODO: should we keep a hash table of field names->escaped field names?
    // If the same fields keep cropping up and have to be escaped again and again it could have
    //   a small but noticeable impact on performance
    for (size_t i=0; i < field_count; i++) {
        auto &field = fields[i];
        dest.push_back('"');
        size_t offset = dest.size();
        details::fmt_helper::append_string_view(field.name, dest);
        details::escape_to_end(dest, offset);
        dest.push_back('"');
        dest.push_back(':');

        bool numeric = is_numeric(field.value_type);
        if (!numeric) {
                dest.push_back('"');
        }
        size_t start_offset = dest.size();
        details::append_value(field, dest);
        details::escape_to_end(dest, start_offset);
        if (!numeric) {
                dest.push_back('"');
        }

        dest.push_back(',');
        dest.push_back(' ');
    }
}


SPDLOG_INLINE void json_formatter::format(const details::log_msg &msg, memory_buf_t &dest)
{
    // TODO: support custom flag formatters
    // TODO: all safe fields can be compiled into one pattern formatter

    memory_buf_t buffer;
    dest.push_back('{');

    for (auto &field_ptr: fields_) {
        field_ptr->format(msg, dest);
    }
    format_data_fields(msg.field_data, msg.field_data_count, dest);

    // Strip trailing space if it's there
    if (dest[dest.size() -1] == ' ') {
        dest.resize(dest.size() -1);
    }
    if (dest[dest.size() -1] == ',') {
        dest.resize(dest.size() -1);
    }
    dest.push_back('}');
    details::fmt_helper::append_string_view(eol_, dest);
}

} // namespace spdlog