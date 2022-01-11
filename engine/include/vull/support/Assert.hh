#pragma once

#ifndef NDEBUG
#define VULL_ASSERTIONS
#define VULL_ASSERTIONS_PEDANTIC
#endif

#define VULL_STRINGIFY_A(x) #x
#define VULL_STRINGIFY(x) VULL_STRINGIFY_A(x)
#define VULL_ENSURE(expr, ...)                                                                                         \
    static_cast<bool>(expr)                                                                                            \
        ? static_cast<void>(0)                                                                                         \
        : vull::fatal_error("Assertion '" #expr "' failed at " __FILE__ ":" VULL_STRINGIFY(__LINE__), ##__VA_ARGS__)

#ifdef VULL_ASSERTIONS
#define VULL_ASSERT(expr, ...) VULL_ENSURE(expr, ##__VA_ARGS__)
#else
#define VULL_ASSERT(expr, ...)
#endif
#ifdef VULL_ASSERTIONS_PEDANTIC
#define VULL_ASSERT_PEDANTIC(expr, ...) VULL_ENSURE(expr, ##__VA_ARGS__)
#else
#define VULL_ASSERT_PEDANTIC(expr, ...)
#endif

#define VULL_ASSERT_NOT_REACHED(...) VULL_ASSERT(false, ##__VA_ARGS__)
#define VULL_ENSURE_NOT_REACHED(...) VULL_ENSURE(false, ##__VA_ARGS__)

namespace vull {

[[noreturn]] void fatal_error(const char *error, const char *note = nullptr);

} // namespace vull
