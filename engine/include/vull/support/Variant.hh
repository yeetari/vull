#pragma once

#include <vull/support/Union.hh>

namespace vull {

template <typename... Ts>
class Variant {
    static_assert(sizeof...(Ts) < 255, "Variant too large");
    static constexpr uint8_t k_null_index = 255u;
    Union<Ts...> m_union;
    uint8_t m_index{k_null_index};

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

    template <typename...>
    void downcast_helper(Variant &) const {}
    template <typename T, typename... Rest>
    void downcast_helper(Variant &new_variant) const {
        if (m_index == index_of<T>()) {
            new_variant.set(m_union.template get<T>());
            return;
        }
        downcast_helper<Rest...>(new_variant);
    }

    template <typename... Us>
    struct Downcast {
        Variant<Us...> operator()(const Variant &variant) {
            Variant<Us...> new_variant;
            variant.downcast_helper<Ts...>(new_variant);
            return new_variant;
        }
    };

    template <typename U>
    struct Downcast<U> {
        U operator()(const Variant &variant) { return variant.get<U>(); }
    };

public:
    Variant() = default;
    template <ContainsType<Ts...> T>
    Variant(const T &value) : m_union(value), m_index(index_of<T>()) {}
    template <ContainsType<Ts...> T>
    Variant(T &&value) : m_union(move(value)), m_index(index_of<T>()) {}
    Variant(const Variant &) = delete;
    Variant(Variant &&) = delete;
    ~Variant() { clear(); }

    Variant &operator=(const Variant &) = delete;
    Variant &operator=(Variant &&) = delete;

    void clear();
    template <ContainsType<Ts...>... Us>
    auto downcast() const;

    template <ContainsType<Ts...> T>
    T &get();
    template <ContainsType<Ts...> T>
    const T &get() const;

    template <ContainsType<Ts...> T>
    bool has() const;

    template <ContainsType<Ts...> T>
    void set(T &&value);

    uint8_t index() const { return m_index; }
};

template <typename... Ts>
void Variant<Ts...>::clear() {
    (maybe_destruct<Ts>() || ...);
    m_index = k_null_index;
}

template <typename... Ts>
template <ContainsType<Ts...>... Us>
auto Variant<Ts...>::downcast() const {
    return Downcast<Us...>{}(*this);
}

template <typename... Ts>
template <ContainsType<Ts...> T>
T &Variant<Ts...>::get() {
    VULL_ASSERT(m_index == index_of<T>());
    return m_union.template get<T>();
}

template <typename... Ts>
template <ContainsType<Ts...> T>
const T &Variant<Ts...>::get() const {
    VULL_ASSERT(m_index == index_of<T>());
    return m_union.template get<T>();
}

template <typename... Ts>
template <ContainsType<Ts...> T>
bool Variant<Ts...>::has() const {
    return m_index == index_of<T>();
}

template <typename... Ts>
template <ContainsType<Ts...> T>
void Variant<Ts...>::set(T &&value) {
    clear();
    m_union.set(forward<T>(value));
    m_index = index_of<T>();
}

} // namespace vull
