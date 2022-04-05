#ifndef SPDLOG_NO_STRUCTURED_SPDLOG

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
    // Can we construct fields with rvalues?
    spdlog::F f_int("var", 1);
    REQUIRE(f_int.value_type == spdlog::FieldValueType::INT);

    spdlog::F f_str_literal("var", "val");
    REQUIRE(f_str_literal.value_type == spdlog::FieldValueType::STRING_VIEW);

    // Can we construct fields with lvalues?
    std::string str1("key");
    std::string str2("str");
    spdlog::F str_f(str1, str2);
    REQUIRE(str_f.value_type == spdlog::FieldValueType::STRING_VIEW);

    // Const chars are troublesome; we really, really want the string literals to
    //   use their compile-time length, and std::strings to use their O(1) run-time
    //   length.  The const char (&val)[N] and string_view constructors take care of
    //   those.
    // If we include a const char * constructor, it will be preferred for string
    //   literals (unless we can arrange some template magic that works for C++11 and
    //   above).  String literals will then have an O(n) cost, and that's no good
    // If we don't include a const char * constructor, the compiler erases const char *
    //   to void *, which for historical resons binds to bool before anything else.
    //   So, for now, const char * maps to bool.  :-|
    const char * cstr = "str";
    auto cstr_f = spdlog::F(cstr, cstr);
    REQUIRE(cstr_f.value_type == spdlog::FieldValueType::BOOL);
}

TEST_CASE("field_logging", "[structured]")
{
    // No fields
    REQUIRE(log_info({}, "Hello") == "Hello");

    // Some fields
    REQUIRE(log_info({F("k", 1)}, "Hello") == "Hello k:1");

    // Test level calls
    spdlog::info({{"field", 1}}, "Hello");
    spdlog::default_logger()->info({{"field", 2}}, "Hello");
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
    logger->flush();
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


TEST_CASE("structured macros", "[structured]")
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
    SPDLOG_DEBUG({F("f",1)}, "Test message 4");
    logger->flush();

    require_message_count(TEST_FILENAME, 2);
    REQUIRE(last_line(file_contents(TEST_FILENAME)) == "Test message 4 f:1");

    spdlog::set_default_logger(std::move(orig_default_logger));
}

TEST_CASE("structured context", "[structured]")
{
    {
        spdlog::context ctx({{"c1","1"}});
        REQUIRE(log_info({}, "Hello") == "Hello c1:1");
    }

    {
        spdlog::context ctx1({{"c1","1"}});
        {
            spdlog::context ctx2({{"c2","2"}});
            REQUIRE(log_info({}, "Hello") == "Hello c2:2 c1:1");
        }
        REQUIRE(log_info({}, "Hello") == "Hello c1:1");
    }

    REQUIRE(log_info({}, "Hello") == "Hello");
}

TEST_CASE("structured snapshots", "[structured]")
{
    enum steps {START, CTX_REPLACED, LOG_IN_THREAD};
    std::atomic<steps> step{steps::START};

    std::unique_ptr<std::thread> thread;
    {
        spdlog::context inner_ctx({{"c1","1"}});
        auto ctx_snapshot=spdlog::snapshot_context_fields();
        thread = spdlog::details::make_unique<std::thread>(
            [ctx_snapshot, &step] {
            spdlog::replacement_context ctx(ctx_snapshot);
            step = steps::CTX_REPLACED;
            while(step != LOG_IN_THREAD) ; // spin until ready
            REQUIRE(log_info({}, "Hello") == "Hello c1:1");
        });
    }
    // inner_ctx should be fully out of scope at this point

    // Wait for the context to be replaced in the thread, but verify
    //   that it doesn't affect the context of the main thread
    while(step != CTX_REPLACED) ;
    spdlog::context main_ctx({{"c2","2"}});
    REQUIRE(log_info({}, "Hello") == "Hello c2:2");

    // Verify that setting the context on the main thread doesn't
    //    effect the test thread
    step = LOG_IN_THREAD;
    thread->join();
}


TEST_CASE("structured snapshots with ctx", "[structured]")
{
    std::unique_ptr<std::thread> thread;
    {
        spdlog::context inner_ctx({{"c1","1"}});
        auto ctx_snapshot=spdlog::snapshot_context_fields();
        thread = spdlog::details::make_unique<std::thread>(
            [ctx_snapshot] {
            {
                spdlog::replacement_context ctx(ctx_snapshot, {{"c2", 2}});
                REQUIRE(log_info({}, "Hello") == "Hello c2:2 c1:1");
            }
            REQUIRE(log_info({}, "Middle") == "Middle");
        });
    }
    thread->join();
    REQUIRE(log_info({}, "Bye") == "Bye");
}


#endif // SPDLOG_NO_STRUCTURED_SPDLOG
