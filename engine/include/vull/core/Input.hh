#pragma once

#include <vull/maths/Vec.hh>
#include <vull/support/Hash.hh>

#include <stdint.h>

namespace vull {

enum class ButtonMask : uint8_t {
    None = 0,
    Left = 1u << 0u,
    Middle = 1u << 1u,
    Right = 1u << 2u,
};
using Button = ButtonMask;

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
using MouseMoveCallback = void(Vec2f delta, Vec2f position, ButtonMask buttons);

template <>
struct Hash<Button> {
    hash_t operator()(Button button, hash_t seed) const { return hash_of(static_cast<uint8_t>(button), seed); }
};

template <>
struct Hash<Key> {
    hash_t operator()(Key key, hash_t seed) const { return hash_of(static_cast<uint8_t>(key), seed); }
};

inline constexpr ButtonMask operator-(ButtonMask mask) {
    return static_cast<ButtonMask>(-static_cast<uint32_t>(mask));
}
inline constexpr ButtonMask operator&(ButtonMask lhs, ButtonMask rhs) {
    return static_cast<ButtonMask>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}
inline constexpr ButtonMask operator|(ButtonMask lhs, ButtonMask rhs) {
    return static_cast<ButtonMask>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}
inline constexpr ButtonMask operator^(ButtonMask lhs, ButtonMask rhs) {
    return static_cast<ButtonMask>(static_cast<uint32_t>(lhs) ^ static_cast<uint32_t>(rhs));
}

inline constexpr ModifierMask operator&(ModifierMask lhs, ModifierMask rhs) {
    return static_cast<ModifierMask>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}
inline constexpr ModifierMask operator|(ModifierMask lhs, ModifierMask rhs) {
    return static_cast<ModifierMask>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

} // namespace vull
