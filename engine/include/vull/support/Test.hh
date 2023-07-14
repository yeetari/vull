#pragma once

#include <vull/container/Vector.hh>
#include <vull/support/Format.hh>
#include <vull/support/String.hh>

class Test;

// NOLINTNEXTLINE
extern vull::Vector<Test *> g_tests;

class Test {
    vull::String m_name;
    void (*m_fn)();

public:
    Test(const char *suite_name, const char *name, void (*fn)())
        : m_name(vull::format("{}.{}", suite_name, name)), m_fn(fn) {
        g_tests.push(this);
    }

    const vull::String &name() const { return m_name; }
    auto fn() const { return m_fn; }
};

class TestFailure {
    const char *m_expr;
    const char *m_file;
    unsigned m_line;

public:
    TestFailure(const char *expr, const char *file, unsigned line) : m_expr(expr), m_file(file), m_line(line) {}

    const char *expr() const { return m_expr; }
    const char *file() const { return m_file; }
    unsigned line() const { return m_line; }
};

#define EXPECT(...)                                                                                                    \
    static_cast<bool>(__VA_ARGS__) ? static_cast<void>(0)                                                              \
                                   : throw TestFailure("EXPECT(" #__VA_ARGS__ ")", __FILE__, __LINE__)

#define TEST_CASE(suite, name)                                                                                         \
    _Pragma("clang diagnostic push");                                                                                  \
    _Pragma("clang diagnostic ignored \"-Wexit-time-destructors\"");                                                   \
    _Pragma("clang diagnostic ignored \"-Wglobal-constructors\"");                                                     \
    _Pragma("clang diagnostic ignored \"-Wmissing-noreturn\"");                                                        \
    static void suite##_##name();                                                                                      \
    static Test s_##name##_registrant(#suite, #name, &(suite##_##name));                                               \
    _Pragma("clang diagnostic pop");                                                                                   \
    static void suite##_##name()
