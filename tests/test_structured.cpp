#include "includes.h"
#include "test_sink.h"

template<typename T, typename... ArgTypes, size_t N>
std::string log_info(const std::array<spdlog::Field, N>& fields, const T &what, ArgTypes&&... args)
{
    std::ostringstream oss;
    auto oss_sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(oss);

    spdlog::logger oss_logger("oss", oss_sink);
    oss_logger.set_pattern("%v");
    oss_logger.slog({}, spdlog::level::info, fields, what, args...);

    return oss.str().substr(0, oss.str().length() - strlen(spdlog::details::os::default_eol));
}

TEST_CASE("fields ", "[field_construction]")
{
    spdlog::F("var", 1);
    spdlog::F("var", "val");

    std::string str1("str");
    spdlog::F(str1, 1);
}

TEST_CASE("field_array ", "[field_array_construction]")
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

TEST_CASE("field_logging ", "[field_logging]")
{
    // No fields
    REQUIRE(log_info(spdlog::NO_FIELDS, "Hello") == "Hello");

    // Some fields
    REQUIRE(log_info(spdlog::build_fields(spdlog::F("k", 1)), "Hello") == "Hello");
}
