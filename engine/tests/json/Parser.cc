#include <vull/json/Parser.hh>

#include <vull/json/Tree.hh>
#include <vull/maths/Epsilon.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Result.hh>
#include <vull/support/Test.hh>

#include <stdint.h>

using namespace vull;

TEST_CASE(JsonParser, Null) {
    auto value = VULL_EXPECT(json::parse("null"));
    EXPECT(value.has<json::Null>());
}

TEST_CASE(JsonParser, True) {
    auto value = VULL_EXPECT(json::parse("true"));
    EXPECT(value.has<bool>());
    EXPECT(VULL_ASSUME(value.get<bool>()));
}

TEST_CASE(JsonParser, False) {
    auto value = VULL_EXPECT(json::parse("false"));
    EXPECT(value.has<bool>());
    EXPECT(!VULL_ASSUME(value.get<bool>()));
}

TEST_CASE(JsonParser, Integer) {
    auto value = VULL_EXPECT(json::parse("300"));
    EXPECT(value.has<int64_t>());
    EXPECT(VULL_ASSUME(value.get<int64_t>()) == 300);
}

TEST_CASE(JsonParser, String) {
    auto value = VULL_EXPECT(json::parse("\"hello\""));
    EXPECT(value.has<json::String>());
    EXPECT(VULL_ASSUME(value.get<json::String>()) == "hello");
}

TEST_CASE(JsonParser, Array1) {
    auto value = VULL_EXPECT(json::parse("[]"));
    EXPECT(value.has<json::Array>());
    EXPECT(VULL_ASSUME(value.get<json::Array>()).empty());
}
TEST_CASE(JsonParser, Array2) {
    auto value = VULL_EXPECT(json::parse("[null]"));
    EXPECT(value.has<json::Array>());
    EXPECT(VULL_ASSUME(value.get<json::Array>()).size() == 1);
    EXPECT(value[0].has<json::Null>());
}
TEST_CASE(JsonParser, Array3) {
    auto value = VULL_EXPECT(json::parse("[\"meaty mike\", \"beefy bill\"]"));
    EXPECT(value.has<json::Array>());
    EXPECT(VULL_ASSUME(value.get<json::Array>()).size() == 2);
    EXPECT(VULL_EXPECT(value[0].get<json::String>()) == "meaty mike");
    EXPECT(VULL_EXPECT(value[1].get<json::String>()) == "beefy bill");
}
TEST_CASE(JsonParser, Array4) {
    auto value = VULL_EXPECT(json::parse("[123,456]"));
    EXPECT(value.has<json::Array>());
    EXPECT(VULL_ASSUME(value.get<json::Array>()).size() == 2);
    EXPECT(VULL_EXPECT(value[0].get<int64_t>()) == 123);
    EXPECT(VULL_EXPECT(value[1].get<int64_t>()) == 456);
}
TEST_CASE(JsonParser, Array5) {
    auto value = VULL_EXPECT(json::parse("[{\"foo\": 5e6,\"bar\": null}, \"hello\"]"));
    EXPECT(value.has<json::Array>());
    EXPECT(vull::fuzzy_equal(VULL_EXPECT(value[0]["foo"].get<double>()), 5e6));
    EXPECT(value[0]["bar"].has<json::Null>());
    EXPECT(VULL_EXPECT(value[1].get<json::String>()) == "hello");
}

TEST_CASE(JsonParser, Object1) {
    auto value = VULL_EXPECT(json::parse("{}"));
    EXPECT(value.has<json::Object>());
    EXPECT(VULL_ASSUME(value.get<json::Object>()).empty());
}
TEST_CASE(JsonParser, Object2) {
    auto value = VULL_EXPECT(json::parse("{\"foo\":\"bar\"}"));
    EXPECT(VULL_EXPECT(value["foo"].get<json::String>()) == "bar");
}
