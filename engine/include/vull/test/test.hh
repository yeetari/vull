#pragma once

#include <vull/container/vector.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>

namespace vull::test {

struct Test {
    const char *name;
    void (*fn)();
    Vector<String> messages;
};

extern Test *g_current_test;

} // namespace vull::test

#define TEST_CASE(suite_, case_)                                                                                       \
    static void suite_##_##case_();                                                                                    \
    [[gnu::section("vull_test_info"), gnu::used]] vull::test::Test g_##suite_##_##case_##_descriptor{                  \
        .name = #suite_ "." #case_,                                                                                    \
        .fn = &(suite_##_##case_),                                                                                     \
    };                                                                                                                 \
    static void suite_##_##case_()
