#pragma once

#include <vull/support/enum.hh>
#include <vull/support/integral.hh>
#include <vull/support/utility.hh>

namespace vull {

template <Enum T>
class FlagBitset {
    using type_t = vull::underlying_type<T>;
    static_assert(vull::is_integral<type_t> && !vull::is_signed<type_t>);

private:
    type_t m_value;

    constexpr FlagBitset(type_t value) : m_value(value) {}
    constexpr type_t flag_bit(T flag) const { return type_t(type_t(1) << vull::to_underlying(flag)); }

public:
    // TODO: This shouldn't exist but older clang seems to complain about it.
    constexpr FlagBitset() : m_value(0) {}

    template <typename... U>
    constexpr FlagBitset(U... flag) requires(vull::is_same<T, U> && ...) : m_value((flag_bit(flag) | ...)) {}

    constexpr operator type_t() const { return m_value; }
    constexpr void set(T flag) { m_value |= flag_bit(flag); }
    constexpr void unset(T flag) { m_value &= ~flag_bit(flag); }
    constexpr bool is_set(T flag) const { return (m_value & flag_bit(flag)) != 0; }
};

} // namespace vull
