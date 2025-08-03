#pragma once

#include <stdint.h>

namespace vull::platform {

enum class Endian {
    Big,
    Little,
};

enum class Target {
    LinuxAmd64,
};

consteval Target target() {
    return Target::LinuxAmd64;
}

consteval Endian endian() {
    switch (target()) {
        using enum Target;
    case LinuxAmd64:
        return Endian::Little;
    }
}

consteval bool is_big_endian() {
    return endian() == Endian::Big;
}

consteval bool is_little_endian() {
    return endian() == Endian::Little;
}

void wake_address_single(uint32_t *address);
void wake_address_all(uint32_t *address);
void wait_address(uint32_t *address, uint32_t expected);

} // namespace vull::platform
