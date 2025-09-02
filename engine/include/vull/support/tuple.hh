#pragma once

#include <vull/support/utility.hh>

#include <stddef.h>

namespace vull {

template <size_t>
struct TupleTag {};

template <size_t I, typename T>
struct TupleElem {
    static T elem_type(TupleTag<I>);
    [[no_unique_address]] T value;
    constexpr decltype(auto) operator[](TupleTag<I>) & { return (value); }
    constexpr decltype(auto) operator[](TupleTag<I>) const & { return (value); }
    constexpr decltype(auto) operator[](TupleTag<I>) && { return vull::move(value); }
    constexpr decltype(auto) operator[](TupleTag<I>) const && { return vull::move(value); }
};

template <typename, typename...>
struct TupleBase;
template <size_t... Is, typename... Ts>
struct TupleBase<IntegerSequence<size_t, Is...>, Ts...> : TupleElem<Is, Ts>... {
    using TupleElem<Is, Ts>::elem_type...;
    using TupleElem<Is, Ts>::operator[]...;

    TupleBase() = default;
    explicit TupleBase(const Ts &...ts) requires(!is_ref<Ts> && ...) : TupleElem<Is, Ts>{ts}... {}
    explicit TupleBase(Ts &&...ts) : TupleElem<Is, Ts>{vull::forward<Ts>(ts)}... {}
};

template <typename... Ts>
using tuple_sequence_t = make_integer_sequence<size_t, sizeof...(Ts)>;

template <typename... Ts>
struct Tuple : TupleBase<tuple_sequence_t<Ts...>, Ts...> {
    using TupleBase<tuple_sequence_t<Ts...>, Ts...>::TupleBase;
};

template <size_t I, typename T>
constexpr decltype(auto) get(T &&tuple) {
    return tuple[TupleTag<I>()];
}

template <typename... Ts>
constexpr auto forward_as_tuple(Ts &&...ts) {
    return Tuple<Ts &&...>(vull::forward<Ts>(ts)...);
}

template <typename... Ts>
constexpr auto make_tuple(Ts &&...ts) {
    return Tuple<vull::decay_unwrap<Ts>...>(vull::forward<Ts>(ts)...);
}

} // namespace vull

namespace std {

template <size_t I, typename T>
struct tuple_element;
template <size_t I, typename... Ts>
struct tuple_element<I, vull::Tuple<Ts...>> {
    using type = decltype(vull::Tuple<Ts...>::elem_type(vull::TupleTag<I>()));
};
template <size_t I, typename... Ts>
struct tuple_element<I, const vull::Tuple<Ts...>> {
    using type = const decltype(vull::Tuple<Ts...>::elem_type(vull::TupleTag<I>()));
};

template <typename T>
struct tuple_size;
template <typename... Ts>
struct tuple_size<vull::Tuple<Ts...>> {
    static constexpr size_t value = sizeof...(Ts);
};
template <typename... Ts>
struct tuple_size<const vull::Tuple<Ts...>> {
    static constexpr size_t value = sizeof...(Ts);
};

} // namespace std
