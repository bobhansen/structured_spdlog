#include "includes.h"
#include "test_sink.h"

#include <limits>

#include "spdlog/details/log_msg_buffer.h"


template<typename T, typename... ArgTypes, size_t N>
std::string log_info(const std::array<spdlog::Field, N>& fields, const T &what, ArgTypes&&... args)
{
    std::ostringstream oss;
    auto oss_sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(oss);

    spdlog::logger oss_logger("oss", oss_sink);
    oss_logger.set_pattern("%v");
    oss_logger.slog({}, spdlog::level::info, fields, what, std::forward<ArgTypes>(args)...);

    return oss.str().substr(0, oss.str().length() - strlen(spdlog::details::os::default_eol));
}

TEST_CASE("fields", "[structured]")
{
    // Can we construct fields of all types with lvalues?
    spdlog::F("var", 1);
    spdlog::F("var", "val");

    // Can we construct fields with rvalues?
    std::string str1("str");
    spdlog::F(str1, str1);

    // Can we construct fields with rvalues?
    const char * cstr = "str";
    auto cstr_f = spdlog::F(cstr, cstr);
    REQUIRE(cstr_f.value_type == spdlog::FieldValueType::INT);
}

TEST_CASE("build_fields", "[structured]")
{
    auto array0 = spdlog::build_fields();
    REQUIRE(array0.size() == 0);

    auto array1 = spdlog::build_fields(spdlog::F("var", 1));
    REQUIRE(array1.size() == 1);
    REQUIRE(to_string(array1[0].name) == "var");
    REQUIRE(array1[0].value_type == spdlog::FieldValueType::INT);
    REQUIRE(array1[0].int_ == 1);

    auto array2 = spdlog::build_fields(spdlog::F("var", 1), spdlog::F("var2", "two"));
    REQUIRE(array2.size() == 2);
    REQUIRE(to_string(array2[0].name) == "var");
    REQUIRE(array2[0].value_type == spdlog::FieldValueType::INT);
    REQUIRE(array2[0].int_ == 1);
    REQUIRE(to_string(array2[1].name) == "var2");
    REQUIRE(array2[1].value_type == spdlog::FieldValueType::STRING_VIEW);
    REQUIRE(array2[1].string_view_ == "two");
}

TEST_CASE("field_logging", "[structured]")
{
    // No fields
    REQUIRE(log_info(spdlog::NO_FIELDS, "Hello") == "Hello");

    // Some fields
    REQUIRE(log_info(spdlog::build_fields(spdlog::F("k", 1)), "Hello") == "Hello");

    // Fields and fmt
    REQUIRE(log_info(spdlog::build_fields(spdlog::F("k", 1)), "Hello {}", "world") == "Hello world");
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
        auto array2 = spdlog::build_fields(spdlog::F("var", 1), spdlog::F("var2", "two"));
        spdlog::details::log_msg test_input1(spdlog::source_loc{}, "name", spdlog::level::info, "msg", array2.data(), array2.size());
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
        logger->slog({}, spdlog::level::info, spdlog::build_fields(spdlog::F("str", test_string)), "test msg");
    }
    REQUIRE(test_sink->msg_counter() < messages);
    REQUIRE(test_sink->msg_counter() > 0);
    REQUIRE(tp->overrun_counter() > 0);
}
