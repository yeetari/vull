#pragma once

namespace vull {

struct LsanDisabler {
    LsanDisabler();
    LsanDisabler(const LsanDisabler &) = delete;
    LsanDisabler(LsanDisabler &&) = delete;
    ~LsanDisabler();

    LsanDisabler &operator=(const LsanDisabler &) = delete;
    LsanDisabler &operator=(LsanDisabler &&) = delete;
};

} // namespace vull
