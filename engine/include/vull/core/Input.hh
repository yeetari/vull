#pragma once

#include <vull/maths/Vec.hh>

#include <stdint.h>

namespace vull {

enum class MouseButtonMask : uint8_t {
    None = 0,
    Left = 1u << 0u,
    Middle = 1u << 1u,
    Right = 1u << 2u,
};
using MouseButton = MouseButtonMask;

enum class Key : uint8_t {
    Unknown = 0,
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    Space,
    Shift,
    Count,
};

enum class ModifierMask : uint8_t {
    Shift = 1u << 0u,
    Ctrl = 1u << 1u,
    Alt = 1u << 2u,
    Super = 1u << 3u,
    CapsLock = 1u << 4u,
};

using KeyCallback = void(ModifierMask mods);
using MouseCallback = void(Vec2f position);
using MouseMoveCallback = void(Vec2f delta, Vec2f position, MouseButtonMask buttons);

inline constexpr MouseButtonMask operator-(MouseButtonMask mask) {
    return static_cast<MouseButtonMask>(-static_cast<uint32_t>(mask));
}
inline constexpr MouseButtonMask operator&(MouseButtonMask lhs, MouseButtonMask rhs) {
    return static_cast<MouseButtonMask>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}
inline constexpr MouseButtonMask operator|(MouseButtonMask lhs, MouseButtonMask rhs) {
    return static_cast<MouseButtonMask>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}
inline constexpr MouseButtonMask operator^(MouseButtonMask lhs, MouseButtonMask rhs) {
    return static_cast<MouseButtonMask>(static_cast<uint32_t>(lhs) ^ static_cast<uint32_t>(rhs));
}

inline constexpr ModifierMask operator&(ModifierMask lhs, ModifierMask rhs) {
    return static_cast<ModifierMask>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}
inline constexpr ModifierMask operator|(ModifierMask lhs, ModifierMask rhs) {
    return static_cast<ModifierMask>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

} // namespace vull
