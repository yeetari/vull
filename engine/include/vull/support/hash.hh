#pragma once

#include <vull/support/enum.hh>
#include <vull/support/integral.hh>
#include <vull/support/span.hh>

#include <stdint.h>

// Forward declarations to avoid including xxhash header.
extern "C" [[gnu::pure]] uint64_t XXH3_64bits(const void *input, size_t length);
extern "C" [[gnu::pure]] uint64_t XXH3_64bits_withSeed(const void *input, size_t length, uint64_t seed);

namespace vull {

// TODO: Use 64-bit hashes?
using hash_t = uint32_t;

template <typename>
struct Hash;

inline hash_t hash_combine(hash_t lhs, hash_t rhs) {
    lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
    return lhs;
}

template <typename T>
hash_t hash_of(const T &object) {
    return Hash<T>{}(object);
}

template <typename T>
hash_t hash_of(const T &object, hash_t seed) {
    return Hash<T>{}(object, seed);
}

template <Integral T>
requires(sizeof(T) <= sizeof(uint32_t)) struct Hash<T> {
    hash_t operator()(T value) const {
        value ^= value >> 15;
        value *= 0x2c1b3c6du;
        value ^= value >> 12;
        value *= 0x297a2d39u;
        value ^= value >> 15;
        return value;
    }
    hash_t operator()(T value, hash_t seed) const { return hash_combine((*this)(value), seed); }
};

template <>
struct Hash<uint64_t> {
    hash_t operator()(uint64_t value) const {
        value = (~value) + (value << 18);
        value ^= value >> 31;
        value *= 21;
        value ^= value >> 11;
        value += value << 6;
        value ^= value >> 22;
        return static_cast<hash_t>(value);
    }
    hash_t operator()(uint64_t value, hash_t seed) const { return hash_combine((*this)(value), seed); }
};

template <Enum T>
struct Hash<T> {
    hash_t operator()(T t) const { return hash_of(vull::to_underlying(t)); }
    hash_t operator()(T t, hash_t seed) const { return hash_of(vull::to_underlying(t), seed); }
};

template <typename T>
struct Hash<Span<T>> {
    hash_t operator()(Span<T> span) const { return static_cast<hash_t>(XXH3_64bits(span.data(), span.size_bytes())); }
    hash_t operator()(Span<T> span, hash_t seed) const {
        return static_cast<hash_t>(XXH3_64bits_withSeed(span.data(), span.size_bytes(), seed));
    }
};

template <>
struct Hash<class StringView> : public Hash<Span<const char>> {};

} // namespace vull
