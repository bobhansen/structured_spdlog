#include "includes.h"
#include "test_sink.h"

#include <limits>

#include "spdlog/details/log_msg_buffer.h"

using spdlog::F;


std::string log_info(std::initializer_list<spdlog::Field> fields, spdlog::string_view_t what)
{
    std::ostringstream oss;
    auto oss_sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(oss);

    spdlog::logger oss_logger("oss", oss_sink);
    oss_logger.set_pattern("%v%V");
    oss_logger.log({}, spdlog::level::info, fields, what);

    return oss.str().substr(0, oss.str().length() - strlen(spdlog::details::os::default_eol));
}

TEST_CASE("fields", "[structured]")
{
    // Can we construct fields of all types with lvalues?
    F("var", 1);
    F("var", "val");

    // Can we construct fields with rvalues?
    std::string str1("str");
    spdlog::F(str1, str1);

    // Can we construct fields with rvalues?
    const char * cstr = "str";
    auto cstr_f = spdlog::F(cstr, cstr);
    REQUIRE(cstr_f.value_type == spdlog::FieldValueType::INT);
}

TEST_CASE("field_logging", "[structured]")
{
    // No fields
    REQUIRE(log_info({}, "Hello") == "Hello");

    // Some fields
    REQUIRE(log_info({F("k", 1)}, "Hello") == "Hello k:1");
}

template<typename T>
void test_numeric_to_string() {
    T zero_val = {};
    REQUIRE(spdlog::details::value_to_string(spdlog::Field("", zero_val)) == std::to_string(zero_val));

    T min_val = std::numeric_limits<T>::min();
    REQUIRE(spdlog::details::value_to_string(spdlog::Field("", min_val)) == std::to_string(min_val));

    T max_val = std::numeric_limits<T>::max();
    REQUIRE(spdlog::details::value_to_string(spdlog::Field("", max_val)) == std::to_string(max_val));

    T lowest_val = std::numeric_limits<T>::lowest();
    REQUIRE(spdlog::details::value_to_string(spdlog::Field("", lowest_val)) == std::to_string(lowest_val));
}

TEST_CASE("to_string", "[structured]")
{
    // Numerics
    test_numeric_to_string<short>();
    test_numeric_to_string<unsigned short>();
    test_numeric_to_string<int>();
    test_numeric_to_string<unsigned int>();
    test_numeric_to_string<long>();
    test_numeric_to_string<unsigned long>();
    test_numeric_to_string<long long>();
    test_numeric_to_string<unsigned long long>();
    test_numeric_to_string<unsigned char>();
    test_numeric_to_string<float>();
    test_numeric_to_string<double>();
    test_numeric_to_string<long double>();

    // string_view
    REQUIRE(spdlog::details::value_to_string(spdlog::Field("", "")) == "");
    REQUIRE(spdlog::details::value_to_string(spdlog::Field("", "data")) == "data");

    // bool
    REQUIRE(spdlog::details::value_to_string(spdlog::Field("", true)) == "true");
    REQUIRE(spdlog::details::value_to_string(spdlog::Field("", false)) == "false");

    // char
    REQUIRE(spdlog::details::value_to_string(spdlog::Field("", 'c')) == "c");

}


TEST_CASE("buffered_msg_field_copies ", "[structured]")
{
    std::unique_ptr<spdlog::details::log_msg_buffer> test1;
    {
        auto array2 = {F("var", 1), F("var2", "two")};
        spdlog::details::log_msg test_input1(spdlog::source_loc{}, "name", spdlog::level::info, "msg", array2.begin(), array2.size());
        test1 = spdlog::details::make_unique<spdlog::details::log_msg_buffer>(test_input1);
    }
    REQUIRE(test1->field_data_count == 2);
    REQUIRE(test1->field_data[0].name == "var");
    REQUIRE(test1->field_data[0].value_type == spdlog::FieldValueType::INT);
    REQUIRE(test1->field_data[0].int_ == 1);
    REQUIRE(test1->field_data[1].name == "var2");
    REQUIRE(test1->field_data[1].value_type == spdlog::FieldValueType::STRING_VIEW);
    REQUIRE(test1->field_data[1].string_view_ == "two");
}

TEST_CASE("async_structured ", "[structured]")
{
    auto test_sink = std::make_shared<spdlog::sinks::test_sink_mt>();
    test_sink->set_delay(std::chrono::milliseconds(1));
    size_t queue_size = 4;
    size_t messages = 1024;

    auto tp = std::make_shared<spdlog::details::thread_pool>(queue_size, 1);
    auto logger = std::make_shared<spdlog::async_logger>("as", test_sink, tp, spdlog::async_overflow_policy::overrun_oldest);
    for (size_t i = 0; i < messages; i++)
    {
        // Build on the stack so it's out of scope
        std::string test_string("abcdefghijklmnopqrstuvwxyz");
        logger->log({}, spdlog::level::info, {F("str", test_string)}, "test msg");
    }
    REQUIRE(test_sink->msg_counter() < messages);
    REQUIRE(test_sink->msg_counter() > 0);
    REQUIRE(tp->overrun_counter() > 0);
}


#if SPDLOG_ACTIVE_LEVEL != SPDLOG_LEVEL_DEBUG
#    error "Invalid SPDLOG_ACTIVE_LEVEL in test. Should be SPDLOG_LEVEL_DEBUG"
#endif

#define TEST_FILENAME "test_logs/structured_macro_log"
std::string last_line(const std::string &str) {
    if (str.empty()) {
        return "";
    }
    auto start = str.begin();
    auto curr = str.end() - 1;

    // Skip last line
    while ((*curr == '\n' || *curr == '\r') && curr != start) {
        curr--;
    }
    auto end = curr + 1; // end is exclusive

    while (curr != start && *(curr-1) != '\n' && *(curr-1) != '\r' && curr != start) {
        curr--;
    }
    return std::string(curr, end);
}


TEST_CASE("structured macros", "[structuredX]")
{
    prepare_logdir();
    spdlog::filename_t filename = SPDLOG_FILENAME_T(TEST_FILENAME);

    auto logger = spdlog::create<spdlog::sinks::basic_file_sink_mt>("logger", filename);
    logger->set_pattern("%v%V");
    logger->set_level(spdlog::level::trace);

    SPDLOG_LOGGER_TRACE(logger, {}, "Test message 1");
    SPDLOG_LOGGER_DEBUG(logger, {F("f", 0)}, "Test message 2");
    logger->flush();

    REQUIRE(last_line(file_contents(TEST_FILENAME)) == "Test message 2 f:0");
    REQUIRE(count_lines(TEST_FILENAME) == 1);

    auto orig_default_logger = spdlog::default_logger();
    spdlog::set_default_logger(logger);

    SPDLOG_TRACE({}, "Test message 3");
    SPDLOG_DEBUG({{"f",1}}, "Test message 4");
    logger->flush();

    require_message_count(TEST_FILENAME, 2);
    REQUIRE(last_line(file_contents(TEST_FILENAME)) == "Test message 4 f:1");

    spdlog::error({F("f",2)}, "Test message 5");
    logger->flush();
    REQUIRE(last_line(file_contents(TEST_FILENAME)) == "Test message 5 f:2");

    spdlog::set_default_logger(std::move(orig_default_logger));
}
