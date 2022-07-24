#pragma once

#include <vull/support/Integral.hh>
#include <vull/support/Span.hh>

#include <stdint.h>

namespace vull {

using hash_t = uint32_t;

template <typename>
struct Hash;

template <Integral T>
struct Hash<T> {
    hash_t operator()(T t, hash_t seed) const { return seed + hash_t(t % UINT32_MAX); }
};

template <typename T, typename SizeT>
struct Hash<Span<T, SizeT>> {
    hash_t operator()(Span<T, SizeT> span, hash_t hash = 0) const {
        for (uint8_t byte : span.template as<const uint8_t>()) {
            hash += hash_t(byte);
            hash += hash << 10u;
            hash ^= hash >> 6u;
        }
        hash += hash << 3u;
        hash ^= hash >> 11u;
        hash += hash << 15u;
        return hash;
    }
};

template <>
struct Hash<class StringView> : public Hash<Span<const char, size_t>> {};

template <typename T>
hash_t hash_of(const T &object, hash_t seed = 0) {
    return Hash<T>{}(object, seed);
}

} // namespace vull
