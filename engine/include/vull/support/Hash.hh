#pragma once

#include <vull/support/Integral.hh>

#include <stdint.h>

namespace vull {

using hash_t = uint32_t;

template <typename>
struct Hash;

template <Integral T>
struct Hash<T> {
    hash_t operator()(T t) const { return t; }
};

} // namespace vull
