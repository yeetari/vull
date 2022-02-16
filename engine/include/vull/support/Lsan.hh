#pragma once

#if !defined(__has_feature)
#define __has_feature(x) 0
#endif

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
#include <sanitizer/lsan_interface.h>
namespace vull {
using LsanDisabler = __lsan::ScopedDisabler;
} // namespace vull
#else
namespace vull {
struct LsanDisabler {
    // NOLINTNEXTLINE: Defining the constructor like this suppresses -Wunused-variable
    LsanDisabler() {}
};
} // namespace vull
#endif
