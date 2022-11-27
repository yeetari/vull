#pragma once

#if defined(__GNUC__) && !defined(__clang__)
#define VULL_IGNORE(expr)                                                                                              \
    {                                                                                                                  \
        auto unused_result = (expr);                                                                                   \
        static_cast<void>(unused_result);                                                                              \
    }
#else
#define VULL_IGNORE(expr) static_cast<void>(expr)
#endif

#if defined(__clang__)
#define VULL_GLOBAL(...)                                                                                               \
    _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Wexit-time-destructors\"")                   \
        _Pragma("clang diagnostic ignored \"-Wglobal-constructors\"") __VA_ARGS__ _Pragma("clang diagnostic pop")
#else
#define VULL_GLOBAL(...) __VA_ARGS__
#endif

namespace vull {
namespace detail {

template <bool B, typename T, typename F>
struct Conditional {
    using type = T;
};
template <typename T, typename F>
struct Conditional<false, T, F> {
    using type = F;
};

template <typename, typename U>
struct CopyConst {
    using type = U;
};
template <typename T, typename U>
struct CopyConst<const T, U> {
    using type = const U;
};

template <typename T>
struct RemoveCv {
    using type = T;
};
template <typename T>
struct RemoveCv<const T> {
    using type = T;
};
template <typename T>
struct RemoveCv<volatile T> {
    using type = T;
};
template <typename T>
struct RemoveCv<const volatile T> {
    using type = T;
};

template <typename T>
struct RemoveRef {
    using type = T;
};
template <typename T>
struct RemoveRef<T &> {
    using type = T;
};
template <typename T>
struct RemoveRef<T &&> {
    using type = T;
};

} // namespace detail

template <bool B, typename T, typename F>
using conditional = typename detail::Conditional<B, T, F>::type;
template <typename T, typename U>
using copy_const = typename detail::CopyConst<T, U>::type;
template <typename T>
using remove_cv = typename detail::RemoveCv<T>::type;
template <typename T>
using remove_ref = typename detail::RemoveRef<T>::type;
template <typename T>
using decay = remove_cv<remove_ref<T>>;

template <typename>
inline constexpr bool is_const = false;
template <typename T>
inline constexpr bool is_const<const T> = true;

template <typename>
inline constexpr bool is_ref = false;
template <typename T>
inline constexpr bool is_ref<T &> = true;
template <typename T>
inline constexpr bool is_ref<T &&> = true;

template <typename, typename>
inline constexpr bool is_same = false;
template <typename T>
inline constexpr bool is_same<T, T> = true;

template <typename T>
inline constexpr bool is_trivially_constructible = __is_trivially_constructible(T);

template <typename T>
inline constexpr bool is_trivially_copyable = __is_trivially_copyable(T);

#if __has_builtin(__is_trivially_destructible)
template <typename T>
inline constexpr bool is_trivially_destructible = __is_trivially_destructible(T);
#elif __has_builtin(__has_trivial_destructor)
template <typename T>
inline constexpr bool is_trivially_destructible = requires(T t) {
    t.~T();
}
&&__has_trivial_destructor(T);
#else
#error
#endif

template <typename T, typename U>
inline constexpr bool is_convertible_to = is_same<T, U> || requires(T obj) {
    static_cast<U>(obj);
};

template <typename T, typename U>
concept ConvertibleTo = is_convertible_to<T, U>;

template <typename T>
T declval();

template <typename T>
constexpr T &&forward(remove_ref<T> &arg) {
    return static_cast<T &&>(arg);
}

template <typename T>
constexpr T &&forward(remove_ref<T> &&arg) {
    return static_cast<T &&>(arg);
}

template <typename T>
constexpr remove_ref<T> &&move(T &&arg) {
    return static_cast<remove_ref<T> &&>(arg);
}

template <typename T, typename U = T>
constexpr T exchange(T &obj, U &&new_value) {
    T old_value = move(obj);
    obj = forward<U>(new_value);
    return old_value;
}

template <typename T>
constexpr void swap(T &lhs, T &rhs) {
    T tmp(move(lhs));
    lhs = move(rhs);
    rhs = move(tmp);
}

// NOLINTBEGIN
template <typename T>
struct AlignedStorage {
    alignas(T) unsigned char data[sizeof(T)];

    template <typename... Args>
    void emplace(Args &&...args) {
        new (data) T(forward<Args>(args)...);
    }

    void set(const T &value) { new (data) T(value); }
    void set(T &&value) { new (data) T(move(value)); }
    void release() { get().~T(); }

    T &get() { return *__builtin_launder(reinterpret_cast<T *>(data)); }
    const T &get() const { return const_cast<AlignedStorage<T> *>(this)->get(); }
};
// NOLINTEND

template <typename... Ts>
struct TypeList {
    template <unsigned, typename, typename...>
    struct Index;

    template <unsigned I, typename T>
    struct Index<I, T> {
        static constexpr auto index = unsigned(-1);
    };

    template <unsigned I, typename T, typename U, typename... Rest>
    struct Index<I, T, U, Rest...> {
        static constexpr auto index = is_same<T, U> ? I : Index<I + 1, T, Rest...>::index;
    };

    template <typename T>
    static consteval bool contains() {
        return (is_same<T, Ts> || ...);
    }

    template <typename T>
    static consteval auto index_of() {
        return Index<0, T, Ts...>::index;
    }
};

// clang-format off
template <typename T, typename... Ts>
concept ContainsType = TypeList<Ts...>::template contains<T>();
// clang-format on

template <typename T>
class RefWrapper {
    T *m_ptr;

public:
    constexpr RefWrapper(T &ref) : m_ptr(&ref) {}
    constexpr operator T &() const { return *m_ptr; }
};

template <typename T>
constexpr auto ref(T &ref) {
    return RefWrapper<T>(ref);
}
template <typename T>
constexpr auto cref(const T &ref) {
    return RefWrapper<const T>(ref);
}

template <typename T>
struct UnrapRefWrapper {
    using type = T;
};
template <typename T>
struct UnrapRefWrapper<RefWrapper<T>> {
    using type = T &;
};

template <typename T>
using unwrap_ref = typename UnrapRefWrapper<T>::type;

template <typename T>
using decay_unwrap = unwrap_ref<decay<T>>;

template <typename I, I... Is>
struct IntegerSequence {};

#if __has_builtin(__make_integer_seq)
template <typename I, I N>
using make_integer_sequence = __make_integer_seq<IntegerSequence, I, N>;
#else
template <typename I, I N>
using make_integer_sequence = IntegerSequence<I, __integer_pack(N)...>;
#endif

inline constexpr auto &operator&=(auto &lhs, auto rhs) {
    return lhs = (lhs & rhs);
}
inline constexpr auto &operator|=(auto &lhs, auto rhs) {
    return lhs = (lhs | rhs);
}
inline constexpr auto &operator^=(auto &lhs, auto rhs) {
    return lhs = (lhs ^ rhs);
}

} // namespace vull

#if !__has_include(<vector>)

// NOLINTNEXTLINE
inline void *operator new(unsigned long, void *ptr) {
    return ptr;
}

// NOLINTNEXTLINE
inline void *operator new[](unsigned long, void *ptr) {
    return ptr;
}

#endif
