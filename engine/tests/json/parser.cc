#include <vull/json/parser.hh>

#include <vull/json/tree.hh>
#include <vull/maths/epsilon.hh>
#include <vull/support/result.hh>
#include <vull/support/string.hh> // IWYU pragma: keep
#include <vull/support/string_view.hh>
#include <vull/test/assertions.hh>
#include <vull/test/json.hh>
#include <vull/test/matchers.hh>
#include <vull/test/test.hh>

#include <stdint.h>

using namespace vull;
using namespace vull::test::matchers;

TEST_CASE(JsonParser, Null) {
    EXPECT_THAT(json::parse("null"), is(success(of_json_null())));
}

TEST_CASE(JsonParser, True) {
    EXPECT_THAT(json::parse("true"), is(success(of_json_value<bool>(equal_to(true)))));
}

TEST_CASE(JsonParser, False) {
    EXPECT_THAT(json::parse("false"), is(success(of_json_value<bool>(equal_to(false)))));
}

TEST_CASE(JsonParser, Integer) {
    EXPECT_THAT(json::parse("300"), is(success(of_json_value<int64_t>(equal_to(300)))));
}

TEST_CASE(JsonParser, Integer_Double) {
    EXPECT_THAT(json::parse("300"), is(success(of_json_value<double>(close_to(300.0)))));
}

TEST_CASE(JsonParser, Double) {
    EXPECT_THAT(json::parse("300.5"), is(success(of_json_value<double>(close_to(300.5)))));
}

TEST_CASE(JsonParser, String) {
    EXPECT_THAT(json::parse("\"hello\""), is(success(of_json_value<String>(equal_to(String("hello"))))));
}

TEST_CASE(JsonParser, Array1) {
    EXPECT_THAT(json::parse("[]"), is(success(of_json_value<json::Array>(empty()))));
}
TEST_CASE(JsonParser, Array2) {
    auto value = VULL_EXPECT(json::parse("[null]"));
    EXPECT_TRUE(value.has<json::Array>());
    EXPECT_TRUE(VULL_ASSUME(value.get<json::Array>()).size() == 1);
    EXPECT_TRUE(value[0].has<json::Null>());
}
TEST_CASE(JsonParser, Array3) {
    auto value = VULL_EXPECT(json::parse("[\"meaty mike\", \"beefy bill\"]"));
    EXPECT_TRUE(value.has<json::Array>());
    EXPECT_TRUE(VULL_ASSUME(value.get<json::Array>()).size() == 2);
    EXPECT_TRUE(VULL_EXPECT(value[0].get<String>()) == "meaty mike");
    EXPECT_TRUE(VULL_EXPECT(value[1].get<String>()) == "beefy bill");
}
TEST_CASE(JsonParser, Array4) {
    auto value = VULL_EXPECT(json::parse("[123,456]"));
    EXPECT_TRUE(value.has<json::Array>());
    EXPECT_TRUE(VULL_ASSUME(value.get<json::Array>()).size() == 2);
    EXPECT_TRUE(VULL_EXPECT(value[0].get<int64_t>()) == 123);
    EXPECT_TRUE(VULL_EXPECT(value[1].get<int64_t>()) == 456);
}
TEST_CASE(JsonParser, Array5) {
    auto value = VULL_EXPECT(json::parse("[{\"foo\": 5e6,\"bar\": null}, \"hello\"]"));
    EXPECT_TRUE(value.has<json::Array>());
    EXPECT_TRUE(vull::fuzzy_equal(VULL_EXPECT(value[0]["foo"].get<double>()), 5e6));
    EXPECT_TRUE(value[0]["bar"].has<json::Null>());
    EXPECT_TRUE(VULL_EXPECT(value[1].get<String>()) == "hello");
}

TEST_CASE(JsonParser, Object1) {
    EXPECT_THAT(json::parse("{}"), is(success(of_json_value<json::Object>(empty()))));
}
TEST_CASE(JsonParser, Object2) {
    auto value = VULL_EXPECT(json::parse("{\"foo\":\"bar\"}"));
    EXPECT_TRUE(VULL_EXPECT(value["foo"].get<String>()) == "bar");
}
