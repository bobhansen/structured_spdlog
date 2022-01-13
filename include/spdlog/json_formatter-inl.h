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

class escaped_message_flag : public spdlog::custom_flag_formatter
{
public:
    void format(const spdlog::details::log_msg &msg, const std::tm &, spdlog::memory_buf_t &dest) override
    {
        size_t start_offset = dest.size();
        fmt_helper::append_string_view(msg.payload, dest);
        escape_to_end(dest, start_offset);
    }

    std::unique_ptr<custom_flag_formatter> clone() const override
    {
        return spdlog::details::make_unique<escaped_message_flag>();
    }
};

class field_data_flag : public spdlog::custom_flag_formatter
{
public:
    void format(const spdlog::details::log_msg &msg, const std::tm &, spdlog::memory_buf_t &dest) override
    {
        if (msg.field_data_count > 0 && dest.size() > 0 && dest[dest.size()-1] != '{') {
            dest.push_back(',');
            dest.push_back(' ');
        }

        // TODO: should we keep a hash table of field names->escaped field names?
        // If the same fields keep cropping up and have to be escaped again and again it could have
        //   a small but noticeable impact on performance
        for (size_t i=0; i < msg.field_data_count; i++) {
            if (i > 0) {
                dest.push_back(',');
                dest.push_back(' ');
            }
            auto &field = msg.field_data[i];
            dest.push_back('"');
            size_t offset = dest.size();
            details::fmt_helper::append_string_view(field.name, dest);
            escape_to_end(dest, offset);
            dest.push_back('"');
            dest.push_back(':');

            switch(field.value.index()) {
                case 0:
                    dest.push_back('"');
                    offset = dest.size();
                    details::fmt_helper::append_string_view(std::get<spdlog::string_view_t>(field.value), dest);
                    escape_to_end(dest, offset);
                    dest.push_back('"');
                    break;
                case 1:
                    fmt_helper::append_int(std::get<int>(field.value), dest);
                    break;
                case 2:
                    //TODO: optimize this and get rid of allocations
                    details::fmt_helper::append_string_view(std::to_string(std::get<double>(field.value)), dest);
                    break;
            }
        }
    }

    std::unique_ptr<custom_flag_formatter> clone() const override
    {
        return spdlog::details::make_unique<field_data_flag>();
    }
};
} // namespace details


SPDLOG_INLINE json_formatter::json_formatter(
    pattern_time_type time_type, std::string eol)
    : escaped_time_field_name_("ts")
    , escaped_message_field_name_("msg")
    , escaped_source_loc_field_name_("source")
    , escaped_level_field_name_("level")
    , escaped_thread_id_field_name_("thread_id")
    , internal_formatter_("", time_type, eol)
{
    internal_formatter_.add_flag<details::escaped_message_flag>('V');
    internal_formatter_.add_flag<details::field_data_flag>('~');
    compile_pattern();
}

SPDLOG_INLINE json_formatter::json_formatter(
    std::string escaped_time_field_name,
    std::string escaped_message_field_name,
    std::string escaped_source_loc_field_name,
    std::string escaped_level_field_name,
    std::string escaped_thread_id_field_name
    )
    : escaped_time_field_name_(escaped_time_field_name)
    , escaped_message_field_name_(escaped_message_field_name)
    , escaped_source_loc_field_name_(escaped_source_loc_field_name)
    , escaped_level_field_name_(escaped_level_field_name)
    , escaped_thread_id_field_name_(escaped_thread_id_field_name)
    , internal_formatter_("")
{
    internal_formatter_.add_flag<details::escaped_message_flag>('V');
    internal_formatter_.add_flag<details::field_data_flag>('~');
    compile_pattern();
}

SPDLOG_INLINE std::unique_ptr<formatter> json_formatter::clone() const
{
    // time_type and eol aren't exposed; clone and overwrite the clone's formatter
    // TODO: be able to set the eol_ and time_format_ when we clone
    return std::make_unique<json_formatter>(
        escaped_time_field_name_, escaped_message_field_name_, escaped_source_loc_field_name_, escaped_level_field_name_, escaped_thread_id_field_name_);
}

SPDLOG_INLINE void json_formatter::compile_pattern()
{
    // Having enumerated fields is a losing battle; we should just have field->pattern, and escape any pattern that isn't
    //     known-good as a post-process.  We need hooks into the pattern formatter to post-process the outputs or walk
    //     each field and append-then-escape

    // TODO: support custom flag formatters

    memory_buf_t buffer;
    buffer.push_back('{');
    if (!escaped_time_field_name_.empty()) {
        buffer.push_back('"');
        details::fmt_helper::append_string_view(escaped_time_field_name_, buffer);
        details::fmt_helper::append_string_view("\":\"%Y-%m-%dT%H:%M:%S.%f%z\", ", buffer);  // don't escape; all values are OK
    }
    if (!escaped_message_field_name_.empty()) {
        buffer.push_back('"');
        details::fmt_helper::append_string_view(escaped_message_field_name_, buffer);
        details::fmt_helper::append_string_view("\":\"%V\", ", buffer);  // %V --> %v with escaping
    }
    if (!escaped_source_loc_field_name_.empty()) {
        buffer.push_back('"');
        details::fmt_helper::append_string_view(escaped_source_loc_field_name_, buffer);
        details::fmt_helper::append_string_view("\":\"%@\", ", buffer);  // TODO: escape source filenames; they can have unicode
    }
    if (!escaped_level_field_name_.empty()) {
        buffer.push_back('"');
        details::fmt_helper::append_string_view(escaped_level_field_name_, buffer);
        details::fmt_helper::append_string_view("\":\"%l\", ", buffer);  // don't escape; all values are OK
    }
    if (!escaped_thread_id_field_name_.empty()) {
        buffer.push_back('"');
        details::fmt_helper::append_string_view(escaped_thread_id_field_name_, buffer);
        details::fmt_helper::append_string_view("\":%t, ", buffer);  // don't escape; integer
    }

    // Strip trailing space if it's there
    if (buffer[buffer.size() -1] == ' ') {
        buffer.resize(buffer.size() -1);
    }
    if (buffer[buffer.size() -1] == ',') {
        buffer.resize(buffer.size() -1);
    }
    // %~ for field data and closing brace
    details::fmt_helper::append_string_view("%~}", buffer);
    internal_formatter_.set_pattern(to_string(buffer));
}

SPDLOG_INLINE void json_formatter::format(const details::log_msg &msg, memory_buf_t &dest)
{
    internal_formatter_.format(msg, dest);
}

} // namespace spdlog
