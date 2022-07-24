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
struct ConditionalImpl {
    using type = T;
};
template <typename T, typename F>
struct ConditionalImpl<false, T, F> {
    using type = F;
};

template <typename, typename U>
struct CopyConstImpl {
    using type = U;
};
template <typename T, typename U>
struct CopyConstImpl<const T, U> {
    using type = const U;
};

template <typename T>
struct RemoveCvImpl {
    using type = T;
};
template <typename T>
struct RemoveCvImpl<const T> {
    using type = T;
};
template <typename T>
struct RemoveCvImpl<volatile T> {
    using type = T;
};
template <typename T>
struct RemoveCvImpl<const volatile T> {
    using type = T;
};

template <typename T>
struct RemoveRefImpl {
    using type = T;
};
template <typename T>
struct RemoveRefImpl<T &> {
    using type = T;
};
template <typename T>
struct RemoveRefImpl<T &&> {
    using type = T;
};

template <typename T>
struct IsConstCheck {
    static constexpr bool value = false;
};
template <typename T>
struct IsConstCheck<const T> {
    static constexpr bool value = true;
};

template <typename>
struct IsRefCheck {
    static constexpr bool value = false;
};
template <typename T>
struct IsRefCheck<T &> {
    static constexpr bool value = true;
};
template <typename T>
struct IsRefCheck<T &&> {
    static constexpr bool value = true;
};

template <typename, typename>
struct IsSameCheck {
    static constexpr bool value = false;
};
template <typename T>
struct IsSameCheck<T, T> {
    static constexpr bool value = true;
};

} // namespace detail

template <bool B, typename T, typename F>
using Conditional = typename detail::ConditionalImpl<B, T, F>::type;

template <typename T, typename U>
using CopyConst = typename detail::CopyConstImpl<T, U>::type;

template <typename T>
inline constexpr bool IsConst = detail::IsConstCheck<T>::value;

template <typename T>
inline constexpr bool IsRef = detail::IsRefCheck<T>::value;

template <typename T, typename U>
inline constexpr bool IsSame = detail::IsSameCheck<T, U>::value;

template <typename T>
inline constexpr bool IsTriviallyConstructible = __is_trivially_constructible(T);

template <typename T>
inline constexpr bool IsTriviallyCopyable = __is_trivially_copyable(T);

template <typename T>
concept Destructible = requires(T t) {
    t.~T();
};

#if defined(__clang__) || defined(_MSC_VER)
template <typename T>
inline constexpr bool IsTriviallyDestructible = __is_trivially_destructible(T);
#elif defined(__GNUC__)
// TODO: Is this completely correct?
template <typename T>
inline constexpr bool IsTriviallyDestructible = Destructible<T> &&__has_trivial_destructor(T);
#endif

template <typename T>
using RemoveCv = typename detail::RemoveCvImpl<T>::type;

template <typename T>
using RemoveRef = typename detail::RemoveRefImpl<T>::type;

template <typename T>
T declval();

template <typename T>
constexpr T &&forward(RemoveRef<T> &arg) {
    return static_cast<T &&>(arg);
}

template <typename T>
constexpr T &&forward(RemoveRef<T> &&arg) {
    return static_cast<T &&>(arg);
}

template <typename T>
constexpr RemoveRef<T> &&move(T &&arg) {
    return static_cast<RemoveRef<T> &&>(arg);
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
