#pragma once

#ifndef NDEBUG
#define ASSERTIONS
#define ASSERTIONS_PEDANTIC
#endif

#define ENSURE(expr) static_cast<bool>(expr) ? static_cast<void>(0) : assertion_failed(__FILE__, __LINE__, #expr)
#ifdef ASSERTIONS
#define ASSERT(expr) ENSURE(expr)
#else
#define ASSERT(expr)
#endif
#ifdef ASSERTIONS_PEDANTIC
#define ASSERT_PEDANTIC(expr) ENSURE(expr)
#else
#define ASSERT_PEDANTIC(expr)
#endif

#define ASSERT_NOT_REACHED() ASSERT(false)
#define ENSURE_NOT_REACHED() ENSURE(false)

[[noreturn]] void assertion_failed(const char *file, unsigned int line, const char *expr);
