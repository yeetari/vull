#pragma once

#include <vull/support/Assert.hh>

template <typename T, typename Kind>
concept HasKind = requires(T *t) {
    static_cast<Kind>(T::k_kind);
};

template <typename, typename Kind>
struct Castable {
    template <typename T>
    const T *as() const requires HasKind<T, Kind>;
    template <typename T>
    const T *as_non_null() const requires HasKind<T, Kind>;

    template <typename T>
    bool is() const requires HasKind<T, Kind>;
};

template <typename Derived, typename Kind>
template <typename T>
const T *Castable<Derived, Kind>::as() const requires HasKind<T, Kind> {
    return is<T>() ? as_non_null<T>() : nullptr;
}

template <typename Derived, typename Kind>
template <typename T>
const T *Castable<Derived, Kind>::as_non_null() const requires HasKind<T, Kind> {
    ASSERT(is<T>());
    ASSERT_PEDANTIC(dynamic_cast<const T *>(static_cast<const Derived *>(this)) != nullptr);
    return static_cast<const T *>(this);
}

template <typename Derived, typename Kind>
template <typename T>
bool Castable<Derived, Kind>::is() const requires HasKind<T, Kind> {
    return static_cast<const Derived *>(this)->m_kind == T::k_kind;
}
