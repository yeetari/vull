#pragma once

#include <vull/support/Utility.hh>

#include <stddef.h>
#include <stdint.h>

namespace vull {

template <typename T, typename SizeT = uint32_t>
class Span {
    T *m_data{nullptr};
    SizeT m_size{0};

    template <typename U>
    using const_t = copy_const<T, U>;

    static constexpr bool is_void = is_same<remove_cv<T>, void>;
    using no_void_t = conditional<is_void, char, T>;

public:
    constexpr Span() = default;
    constexpr Span(T *data, SizeT size) : m_data(data), m_size(size) {}
    constexpr Span(no_void_t &object) requires(!is_void) : m_data(&object), m_size(1) {}

    template <typename U, typename SizeU = SizeT>
    constexpr Span<U, SizeU> as() const;
    constexpr const_t<uint8_t> *byte_offset(SizeT offset) const;

    // Allow implicit conversion from `Span<T>` to `Span<void>`.
    constexpr operator Span<void, SizeT>() const requires(!is_const<T>) { return {data(), size_bytes()}; }
    constexpr operator Span<const void, SizeT>() const requires(!is_void) { return {data(), size_bytes()}; }
    constexpr operator Span<const T, SizeT>() const { return {data(), size_bytes()}; }

    constexpr T *begin() const { return m_data; }
    constexpr T *end() const { return m_data + m_size; }

    template <typename U = no_void_t>
    constexpr U &operator[](SizeT index) const requires(!is_void);
    constexpr bool empty() const { return m_size == 0; }
    constexpr T *data() const { return m_data; }
    constexpr SizeT size() const { return m_size; }
    constexpr SizeT size_bytes() const { return m_size * sizeof(T); }
};

template <typename T>
using LargeSpan = Span<T, size_t>;

template <typename T, typename SizeT>
template <typename U, typename SizeU>
constexpr Span<U, SizeU> Span<T, SizeT>::as() const {
    return {reinterpret_cast<U *>(m_data), static_cast<SizeU>(m_size)};
}

template <typename T, typename SizeT>
constexpr typename Span<T, SizeT>::template const_t<uint8_t> *Span<T, SizeT>::byte_offset(SizeT offset) const {
    return as<const_t<uint8_t>>().data() + offset;
}

template <typename T, typename SizeT>
template <typename U>
constexpr U &Span<T, SizeT>::operator[](SizeT index) const requires(!is_void) {
    return begin()[index];
}

} // namespace vull
