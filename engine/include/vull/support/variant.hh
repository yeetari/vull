#pragma once

#include <vull/support/assert.hh>
#include <vull/support/optional.hh>
#include <vull/support/union.hh>
#include <vull/support/utility.hh>

#include <stdint.h>

namespace vull {
namespace detail {

// Logic for determining the best variant element type from a given type by using the compiler's overload resolution.

template <typename...>
struct all_overloads_t;

template <>
struct all_overloads_t<> {
    void operator()() const;
};

template <typename T, typename... Ts>
struct all_overloads_t<T, Ts...> : all_overloads_t<Ts...> {
    using all_overloads_t<Ts...>::operator();
    T operator()(T) const;
};

template <typename T, typename... Ts>
using best_match_t = decltype(vull::declval<all_overloads_t<Ts...>>()(vull::declval<T &&>()));

} // namespace detail

// NOLINTBEGIN(cppcoreguidelines-special-member-functions)
template <typename... Ts>
class Variant {
    static_assert(sizeof...(Ts) < 255, "Variant too large");
    template <typename...>
    friend class Variant;

private:
    Union<Ts...> m_union;
    uint8_t m_index;

    template <typename T>
    static consteval auto index_of() {
        return TypeList<Ts...>::template index_of<T>();
    }

    template <typename T>
    bool maybe_destruct() {
        if (m_index != index_of<T>()) {
            return false;
        }
        m_union.template release<T>();
        return true;
    }

    template <typename T, typename... Us>
    bool maybe_move(Variant<Us...> &from) {
        if (!from.template has<T>()) {
            return false;
        }
        m_union.template set<T>(vull::move(from.m_union.template get<T>()));
        m_index = index_of<T>();
        return true;
    }

    template <typename T, typename... Us>
    bool maybe_downcast(Variant<Us...> &new_variant) const {
        if (m_index != index_of<T>()) {
            return false;
        }
        if constexpr (TypeList<Us...>::template contains<T>()) {
            new_variant.m_union.template set<T>(m_union.template get<T>());
            new_variant.m_index = Variant<Us...>::template index_of<T>();
            return true;
        }
        return false;
    }

    template <typename... Us>
    struct Downcast {
        Variant<Us...> operator()(const Variant &variant) {
            Variant<Us...> new_variant;
            [[maybe_unused]] bool success = (variant.maybe_downcast<Ts, Us...>(new_variant) || ...);
            VULL_ASSERT(success);
            return new_variant;
        }
    };

    template <typename U>
    struct Downcast<U> {
        U operator()(const Variant &variant) { return variant.get<U>(); }
    };

    Variant() = default;

public:
    template <typename T, typename U = detail::best_match_t<T, Ts...>>
    Variant(T &&value) requires(!is_same<decay<T>, Variant>)
        : m_union(union_tag_t<U>{}, forward<T>(value)), m_index(index_of<U>()) {}

    Variant(const Variant &) = delete;
    template <ContainsType<Ts...>... Us>
    Variant(Variant<Us...> &&);
    ~Variant();

    Variant &operator=(const Variant &) = delete;
    Variant &operator=(Variant &&);

    template <ContainsType<Ts...>... Us>
    auto downcast() const;

    template <ContainsType<Ts...> T>
    bool has() const;

    template <ContainsType<Ts...> T>
    T &get();
    template <ContainsType<Ts...> T>
    const T &get() const;

    template <ContainsType<Ts...> T>
    Optional<T &> try_get();
    template <ContainsType<Ts...> T>
    Optional<const T &> try_get() const;

    template <typename T, typename U = detail::best_match_t<T, Ts...>>
    void set(T &&value);

    uint8_t index() const { return m_index; }
};
// NOLINTEND(cppcoreguidelines-special-member-functions)

template <typename... Ts>
template <ContainsType<Ts...>... Us>
Variant<Ts...>::Variant(Variant<Us...> &&other) {
    (maybe_move<Us, Us...>(other) || ...);
}

template <typename... Ts>
Variant<Ts...>::~Variant() {
    (maybe_destruct<Ts>() || ...);
}

template <typename... Ts>
Variant<Ts...> &Variant<Ts...>::operator=(Variant &&other) {
    if (this != &other) {
        (maybe_destruct<Ts>() || ...);
        (maybe_move<Ts>(other) || ...);
    }
    return *this;
}

template <typename... Ts>
template <ContainsType<Ts...>... Us>
auto Variant<Ts...>::downcast() const {
    return Downcast<Us...>{}(*this);
}

template <typename... Ts>
template <ContainsType<Ts...> T>
bool Variant<Ts...>::has() const {
    return m_index == index_of<T>();
}

template <typename... Ts>
template <ContainsType<Ts...> T>
T &Variant<Ts...>::get() {
    VULL_ASSERT(has<T>());
    return m_union.template get<T>();
}

template <typename... Ts>
template <ContainsType<Ts...> T>
const T &Variant<Ts...>::get() const {
    VULL_ASSERT(has<T>());
    return m_union.template get<T>();
}

template <typename... Ts>
template <ContainsType<Ts...> T>
Optional<T &> Variant<Ts...>::try_get() {
    return has<T>() ? m_union.template get<T>() : Optional<T &>();
}

template <typename... Ts>
template <ContainsType<Ts...> T>
Optional<const T &> Variant<Ts...>::try_get() const {
    return has<T>() ? m_union.template get<T>() : Optional<const T &>();
}

template <typename... Ts>
template <typename T, typename U>
void Variant<Ts...>::set(T &&value) {
    (maybe_destruct<Ts>() || ...);
    m_union.template set<U>(forward<T>(value));
    m_index = index_of<U>();
}

} // namespace vull
