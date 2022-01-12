#include "includes.h"
#include "test_sink.h"

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

TEST_CASE("fields ", "[structured]")
{
    // Can we construct fields of all types with lvalues?
    spdlog::F("var", 1);
    spdlog::F("var", "val");

    // Can we construct fields with rvalues?
    std::string str1("str");
    spdlog::F(str1, str1);
}

TEST_CASE("build_fields ", "[structured]")
{
    auto array0 = spdlog::build_fields();
    REQUIRE(array0.size() == 0);

    auto array1 = spdlog::build_fields(spdlog::F("var", 1));
    REQUIRE(array1.size() == 1);
    REQUIRE(to_string(array1[0].name) == "var");
    REQUIRE(std::get<int>(array1[0].value) == 1);

    auto array2 = spdlog::build_fields(spdlog::F("var", 1), spdlog::F("var2", "two"));
    REQUIRE(array2.size() == 2);
    REQUIRE(to_string(array2[0].name) == "var");
    REQUIRE(std::get<int>(array2[0].value) == 1);
    REQUIRE(to_string(array2[1].name) == "var2");
    REQUIRE(to_string(std::get<spdlog::string_view_t>(array2[1].value)) == "two");
}

TEST_CASE("field_logging ", "[structured]")
{
    // No fields
    REQUIRE(log_info(spdlog::NO_FIELDS, "Hello") == "Hello");

    // Some fields
    REQUIRE(log_info(spdlog::build_fields(spdlog::F("k", 1)), "Hello") == "Hello");

    // Fields and fmt
    REQUIRE(log_info(spdlog::build_fields(spdlog::F("k", 1)), "Hello {}", "world") == "Hello world");
}

TEST_CASE("buffered_msg_field_copies ", "[structured]")
{
    std::unique_ptr<spdlog::details::log_msg_buffer> test1;
    {
        auto array2 = spdlog::build_fields(spdlog::F("var", 1), spdlog::F("var2", "two"));
        spdlog::details::log_msg test_input1(spdlog::source_loc{}, "name", spdlog::level::info, "msg", array2.data(), array2.size());
        test1 = std::make_unique<spdlog::details::log_msg_buffer>(test_input1);
    }
    REQUIRE(test1->field_data_count == 2);
    REQUIRE(test1->field_data[0].name == "var");
    REQUIRE(std::get<int>(test1->field_data[0].value) == 1);
    REQUIRE(test1->field_data[1].name == "var2");
    REQUIRE(std::get<spdlog::string_view_t>(test1->field_data[1].value) == "two");
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
