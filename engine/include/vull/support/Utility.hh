#pragma once

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
