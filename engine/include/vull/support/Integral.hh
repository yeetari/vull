#pragma once

#include <stdint.h>

namespace vull {

template <typename>
inline constexpr bool IsIntegral = false;
template <>
inline constexpr bool IsIntegral<bool> = true;
template <>
inline constexpr bool IsIntegral<char> = true;
template <>
inline constexpr bool IsIntegral<int8_t> = true;
template <>
inline constexpr bool IsIntegral<int16_t> = true;
template <>
inline constexpr bool IsIntegral<int32_t> = true;
template <>
inline constexpr bool IsIntegral<int64_t> = true;
template <>
inline constexpr bool IsIntegral<uint8_t> = true;
template <>
inline constexpr bool IsIntegral<uint16_t> = true;
template <>
inline constexpr bool IsIntegral<uint32_t> = true;
template <>
inline constexpr bool IsIntegral<uint64_t> = true;

template <typename T>
concept Integral = IsIntegral<T>;

} // namespace vull
