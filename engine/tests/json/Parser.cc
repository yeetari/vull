#include <vull/json/Parser.hh>

#include <vull/json/Tree.hh>
#include <vull/maths/Epsilon.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Result.hh>
#include <vull/support/Test.hh>

using namespace vull;

TEST_SUITE(JsonParser, {
    ;
    TEST_CASE(String) {
        auto value = VULL_EXPECT(json::parse("\"hello\""));
        EXPECT(value.has<json::String>());
        EXPECT(VULL_ASSUME(value.get<json::String>()) == "hello");
    }

    TEST_CASE(True) {
        auto value = VULL_EXPECT(json::parse("true"));
        EXPECT(value.has<bool>());
        EXPECT(VULL_ASSUME(value.get<bool>()));
    }

    TEST_CASE(False) {
        auto value = VULL_EXPECT(json::parse("false"));
        EXPECT(value.has<bool>());
        EXPECT(!VULL_ASSUME(value.get<bool>()));
    }

    TEST_CASE(Array1) {
        auto value = VULL_EXPECT(json::parse("[]"));
        EXPECT(value.has<json::Array>());
        EXPECT(VULL_ASSUME(value.get<json::Array>()).empty());
    }
    TEST_CASE(Array2) {
        auto value = VULL_EXPECT(json::parse("[null]"));
        EXPECT(value.has<json::Array>());
        EXPECT(VULL_ASSUME(value.get<json::Array>()).size() == 1);
        EXPECT(value[0].has<json::Null>());
    }
    TEST_CASE(Array3) {
        auto value = VULL_EXPECT(json::parse("[\"meaty mike\", \"beefy bill\"]"));
        EXPECT(value.has<json::Array>());
        EXPECT(VULL_ASSUME(value.get<json::Array>()).size() == 2);
        EXPECT(VULL_EXPECT(value[0].get<json::String>()) == "meaty mike");
        EXPECT(VULL_EXPECT(value[1].get<json::String>()) == "beefy bill");
    }
    TEST_CASE(Array4) {
        auto value = VULL_EXPECT(json::parse("[123,456]"));
        EXPECT(value.has<json::Array>());
        EXPECT(VULL_ASSUME(value.get<json::Array>()).size() == 2);
        EXPECT(vull::fuzzy_equal(VULL_EXPECT(value[0].get<json::Number>()), 123.0));
        EXPECT(vull::fuzzy_equal(VULL_EXPECT(value[1].get<json::Number>()), 456.0));
    }
    TEST_CASE(Array5) {
        auto value = VULL_EXPECT(json::parse("[{\"foo\": 5e6,\"bar\": null}, \"hello\"]"));
        EXPECT(value.has<json::Array>());
        EXPECT(vull::fuzzy_equal(VULL_EXPECT(value[0]["foo"].get<json::Number>()), 5e6));
        EXPECT(value[0]["bar"].has<json::Null>());
        EXPECT(VULL_EXPECT(value[1].get<json::String>()) == "hello");
    }

    TEST_CASE(Object1) {
        auto value = VULL_EXPECT(json::parse("{}"));
        EXPECT(value.has<json::Object>());
        EXPECT(VULL_ASSUME(value.get<json::Object>()).empty());
    }
    TEST_CASE(Object2) {
        auto value = VULL_EXPECT(json::parse("{\"foo\":\"bar\"}"));
        EXPECT(VULL_EXPECT(value["foo"].get<json::String>()) == "bar");
    }
})
