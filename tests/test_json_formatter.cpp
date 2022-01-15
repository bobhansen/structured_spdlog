#include "includes.h"
#include "test_sink.h"

#include <spdlog/details/fmt_helper.h>

using spdlog::memory_buf_t;
using spdlog::NO_FIELDS;
using spdlog::json_field_type;
using Catch::Matchers::Matches;

// log to str and return it
struct field_patterns {
    std::string name;
    std::string pattern;
    spdlog::json_field_type field_type;

    field_patterns(const std::string &_name, const std::string &_pattern, spdlog::json_field_type _field_type = json_field_type::STRING) :
        name(_name), pattern(_pattern), field_type(_field_type) {}
};
using P = field_patterns;

template<typename... Args, size_t N>
static std::string log_to_str(const std::string &msg, const std::array<spdlog::Field,N>& fields,
    std::initializer_list<field_patterns> patterns, bool use_default_patterns = false)
{
    std::ostringstream oss;
    auto oss_sink = std::make_shared<spdlog::sinks::ostream_sink_st>(oss);
    spdlog::logger oss_logger("json_tester", oss_sink);
    oss_logger.set_level(spdlog::level::info);

    auto formatter = new spdlog::json_formatter;
    if (!use_default_patterns) {
        for (auto &pattern: patterns) {
            formatter->add_field(pattern.name, pattern.pattern, pattern.field_type);
        }
    } else {
        formatter->add_default_fields();
    }
    oss_logger.set_formatter(std::unique_ptr<spdlog::formatter>(formatter));
    oss_logger.slog(spdlog::source_loc{"source.cpp", 99, "fn"}, spdlog::level::info, fields, msg);

    return oss.str().substr(0, oss.str().length() - strlen(spdlog::details::os::default_eol));
}

TEST_CASE("json basic output", "[json_formatter]")
{
    // // Test fields with static outputs
    REQUIRE(log_to_str("hello", NO_FIELDS, {P{"MSG", "%v"}, P{"SRC", "%@"}, P{"LEVEL", "%l"}}) == R"({"MSG":"hello", "SRC":"source.cpp:99", "LEVEL":"info"})");

    // // Tests with regex outputs
    REQUIRE_THAT(log_to_str("hello", NO_FIELDS, {{"THREAD","%t", json_field_type::NUMERIC}}), Matches(R"(\{"THREAD":[0-9]+\})"));
    // // ISO8601 regex lifted from https://www.myintervals.com/blog/2009/05/20/iso-8601-date-validation-that-doesnt-suck/
    // //   Trimmed of leading ^ and trailing $
    constexpr char ISO8601_REGEX[] = R"(([\+-]?\d{4}(?!\d{2}\b))((-?)((0[1-9]|1[0-2])(\3([12]\d|0[1-9]|3[01]))?|W([0-4]\d|5[0-2])(-?[1-7])?|(00[1-9]|0[1-9]\d|[12]\d{2}|3([0-5]\d|6[1-6])))([T\s]((([01]\d|2[0-3])((:?)[0-5]\d)?|24\:?00)([\.,]\d+(?!:))?)?(\17[0-5]\d([\.,]\d+)?)?([zZ]|([\+-])([01]\d|2[0-3]):?([0-5]\d)?)?)?)?)";
    std::string time_output_regex = std::string(R"(\{"TM":")") + std::string(ISO8601_REGEX) + std::string(R"("\})");
    REQUIRE_THAT(log_to_str("hello", NO_FIELDS, {{"TM", spdlog::ISO8601_FLAGS}}), Matches(time_output_regex));

    // Fields alone
    auto fields = spdlog::build_fields(spdlog::F("f1", 1), spdlog::F("f2", "two"), spdlog::F("f3", 3.0), spdlog::F("f4",true));
    REQUIRE(log_to_str("hello", fields, {}) == R"({"f1":1, "f2":"two", "f3":3.000000, "f4":true})");

    // Fields with message
    REQUIRE(log_to_str("hello", fields, {{"MSG", "%v"}}) == R"({"MSG":"hello", "f1":1, "f2":"two", "f3":3.000000, "f4":true})");

    // Default output
    static const std::string DEFAULT_RESULT_REGEX = std::string("\\{") +
        R"("time":")" + ISO8601_REGEX + R"(", )" +
        R"("level":"info", )" +
        R"("msg":"hello", )" +
        R"("src_loc":"source.cpp:99", )" +
        R"("f1":1, )" +
        R"("f2":"two", )" +
        R"("f3":3.0+, )" +
        R"("f4":true})";
    REQUIRE_THAT(log_to_str("hello", fields, {}, true), Matches(DEFAULT_RESULT_REGEX));
}

TEST_CASE("json escaped output", "[json_formatter]")
{
    REQUIRE(log_to_str("hello_\x1a", NO_FIELDS, {{"MSG", "%v"}}) == R"({"MSG":"hello_\u001a"})");
    REQUIRE(log_to_str("hello", NO_FIELDS, {{"MSG_\x1a", "%v"}}) == R"({"MSG_\u001a":"hello"})");

    auto fields = spdlog::build_fields(spdlog::F("hello_\x1a", "goodbye_\x1b"));
    REQUIRE(log_to_str("", fields, {}) == R"({"hello_\u001a":"goodbye_\u001b"})");
}

TEST_CASE("json escaping", "[json_formatter]")
{
    // No escaping
    memory_buf_t buffer;
    memset(&buffer[0], '~', buffer.capacity());
    spdlog::details::fmt_helper::append_string_view("hello", buffer);
    spdlog::details::escape_to_end(buffer, 0);
    REQUIRE(to_string(buffer) == "hello");

    // Escaping in the middle
    buffer.resize(0);
    memset(&buffer[0], '~', buffer.capacity());
    spdlog::details::fmt_helper::append_string_view("hello_\x1d_goodbye", buffer);
    spdlog::details::escape_to_end(buffer, 0);
    REQUIRE(to_string(buffer) == "hello_\\u001d_goodbye");

    // Escaping at beginning and end
    buffer.resize(0);
    memset(&buffer[0], '~', buffer.capacity());
    spdlog::details::fmt_helper::append_string_view("\x1d_hello_\x1a", buffer);
    spdlog::details::escape_to_end(buffer, 0);
    REQUIRE(to_string(buffer) == "\\u001d_hello_\\u001a");

    // Wholly escaped
    buffer.resize(0);
    memset(&buffer[0], '~', buffer.capacity());
    spdlog::details::fmt_helper::append_string_view("\x1d", buffer);
    spdlog::details::escape_to_end(buffer, 0);
    REQUIRE(to_string(buffer) == "\\u001d");

    // Null string
    buffer.resize(0);
    memset(&buffer[0], '~', buffer.capacity());
    spdlog::details::fmt_helper::append_string_view("", buffer);
    spdlog::details::escape_to_end(buffer, 0);
    REQUIRE(to_string(buffer) == "");

    // Special characters
    buffer.resize(0);
    memset(&buffer[0], '~', buffer.capacity());
    spdlog::details::fmt_helper::append_string_view("\\\r\n\t\b\f\"", buffer);
    spdlog::details::escape_to_end(buffer, 0);
    REQUIRE(to_string(buffer) == "\\\\\\r\\n\\t\\b\\f\\\"");

    // Not escaped
    buffer.resize(256 - 39);
    char *p = &buffer[0];
    for (int c=0; c <= 255; c++) {
        if (c <= 0x1f || c == '"' || c == '\\' || c == '\b' || c == '\f' || c == '\n' || c == '\r' || c == '\t')
            continue;
        *(p++) = static_cast<char>(c);
    }
    auto before = to_string(buffer);
    spdlog::details::escape_to_end(buffer, 0);
    REQUIRE(to_string(buffer) == before);

    // Skipping alread-esacped parts
    buffer.resize(0);
    memset(&buffer[0], '~', buffer.capacity());
    std::string already_done{"\\\"foo\": \""};
    spdlog::details::fmt_helper::append_string_view(already_done, buffer);
    spdlog::details::fmt_helper::append_string_view("bar\n", buffer);
    spdlog::details::escape_to_end(buffer, already_done.size());
    REQUIRE(to_string(buffer) == "\\\"foo\": \"bar\\n");

}

TEST_CASE("json pattern needs escaping", "[json_formatter]")
{
    REQUIRE(spdlog::details::pattern_needs_escaping("%v") == true); // messages might be unicode
    REQUIRE(spdlog::details::pattern_needs_escaping("%s") == true); // source files might be unicode
    REQUIRE(spdlog::details::pattern_needs_escaping("%%") == false);
    REQUIRE(spdlog::details::pattern_needs_escaping("") == false);
    REQUIRE(spdlog::details::pattern_needs_escaping("no pattern text") == false);
    REQUIRE(spdlog::details::pattern_needs_escaping(spdlog::ISO8601_FLAGS) == false);
}
