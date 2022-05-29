#pragma once

#include <vull/support/Utility.hh>

#if __has_include(<utility>)
#include <utility>
#endif

#include <stddef.h>

namespace vull {

template <typename I, I... Is>
struct IntegerSequence {};

template <size_t... Is>
using IndexSequence = IntegerSequence<size_t, Is...>;

#if __has_builtin(__make_integer_seq)
template <typename I, I N>
using make_integer_sequence = __make_integer_seq<IntegerSequence, I, N>;
#else
template <typename I, I N>
using make_integer_sequence = IntegerSequence<I, __integer_pack(N)...>;
#endif

template <size_t N>
using make_index_sequence = make_integer_sequence<size_t, N>;

template <size_t>
struct TupleTag {};

template <size_t I, typename T>
class TupleElem {
    [[no_unique_address]] T m_value;

public:
    static T elem_type(TupleTag<I>);

    constexpr TupleElem() : m_value{} {}
    constexpr TupleElem(T &&value) : m_value(forward<T &&>(value)) {}
    constexpr decltype(auto) operator[](TupleTag<I>) { return (m_value); }
};

template <typename... Ts>
struct TupleMap : Ts... {
    using Ts::elem_type...;
    using Ts::operator[]...;
};

template <typename, typename...>
struct TupleBase;
template <size_t... Is, typename... Ts>
struct TupleBase<IndexSequence<Is...>, Ts...> : TupleMap<TupleElem<Is, Ts>...> {};

template <typename... Ts>
struct Tuple : TupleBase<make_index_sequence<sizeof...(Ts)>, Ts...> {};

template <typename... Ts>
Tuple(Ts...) -> Tuple<Ts...>;

template <size_t I, typename T>
constexpr decltype(auto) get(T &&tuple) {
    return tuple[TupleTag<I>()];
}

} // namespace vull

namespace std {

#if !__has_include(<utility>)

template <size_t I, typename T>
struct tuple_element;
template <typename T>
struct tuple_size;

#endif

template <size_t I, typename... Ts>
struct tuple_element<I, vull::Tuple<Ts...>> {
    using type = decltype(vull::Tuple<Ts...>::elem_type(vull::TupleTag<I>()));
};

template <typename... Ts>
struct tuple_size<vull::Tuple<Ts...>> {
    static constexpr size_t value = sizeof...(Ts);
};

} // namespace std
