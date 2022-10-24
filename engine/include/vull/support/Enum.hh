#pragma once

namespace vull {

template <typename T>
inline constexpr bool is_enum = __is_enum(T);

template <typename T>
concept Enum = is_enum<T>;

template <Enum E>
using underlying_type = __underlying_type(E);

template <Enum E>
constexpr auto to_underlying(E value) {
    return static_cast<underlying_type<E>>(value);
}

} // namespace vull
