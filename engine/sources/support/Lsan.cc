#include <vull/support/Lsan.hh>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-identifier"
extern "C" void __lsan_disable(); // NOLINT
extern "C" void __lsan_enable();  // NOLINT
extern "C" [[gnu::weak]] void __lsan_disable() {}
extern "C" [[gnu::weak]] void __lsan_enable() {}
#pragma clang diagnostic pop

namespace vull {

LsanDisabler::LsanDisabler() {
    __lsan_disable();
}

LsanDisabler::~LsanDisabler() {
    __lsan_enable();
}

} // namespace vull
