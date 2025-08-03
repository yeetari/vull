#pragma once

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

} // namespace vull::platform
