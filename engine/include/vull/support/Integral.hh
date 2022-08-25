#pragma once

#include <stdint.h>

namespace vull {

template <typename>
inline constexpr bool is_integral = false;
template <>
inline constexpr bool is_integral<bool> = true;
template <>
inline constexpr bool is_integral<char> = true;
template <>
inline constexpr bool is_integral<int8_t> = true;
template <>
inline constexpr bool is_integral<int16_t> = true;
template <>
inline constexpr bool is_integral<int32_t> = true;
template <>
inline constexpr bool is_integral<int64_t> = true;
template <>
inline constexpr bool is_integral<uint8_t> = true;
template <>
inline constexpr bool is_integral<uint16_t> = true;
template <>
inline constexpr bool is_integral<uint32_t> = true;
template <>
inline constexpr bool is_integral<uint64_t> = true;

template <typename T>
concept Integral = is_integral<T>;

} // namespace vull
