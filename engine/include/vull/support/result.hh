#pragma once

#include <vull/support/assert.hh>
#include <vull/support/optional.hh>
#include <vull/support/utility.hh>
#include <vull/support/variant.hh>

// TODO: Find a way to avoid using statement-expressions?
#define VULL_ASSUME(expr, ...)                                                                                         \
    vull::maybe_unwrap(({                                                                                              \
        auto _result_assume = (expr);                                                                                  \
        VULL_ASSERT(!_result_assume.is_error() __VA_OPT__(, ) __VA_ARGS__);                                            \
        _result_assume.disown_value();                                                                                 \
    }))

#define VULL_EXPECT(expr, ...)                                                                                         \
    vull::maybe_unwrap(({                                                                                              \
        auto _result_expect = (expr);                                                                                  \
        VULL_ENSURE(!_result_expect.is_error() __VA_OPT__(, ) __VA_ARGS__);                                            \
        _result_expect.disown_value();                                                                                 \
    }))

#define VULL_TRY(expr)                                                                                                 \
    vull::maybe_unwrap(({                                                                                              \
        auto _result_try = (expr);                                                                                     \
        if (_result_try.is_error()) {                                                                                  \
            return _result_try.error();                                                                                \
        }                                                                                                              \
        _result_try.disown_value();                                                                                    \
    }))

namespace vull {

template <typename T, typename... Es>
class [[nodiscard]] Result {
    static constexpr bool is_void = is_same<decay<T>, void>;
    using storage_t = conditional<is_void, char, conditional<is_ref<T>, RefWrapper<remove_ref<T>>, T>>;
    Variant<storage_t, Es...> m_value;

public:
    // NOLINTNEXTLINE
    Result() requires(is_void) : m_value(storage_t{}) {}

    template <typename U>
    Result(U &&value) : m_value(vull::forward<U>(value)) {}

    // Error variant upcast.
    template <ContainsType<Es...>... Fs>
    Result(Variant<Fs...> &&error) : m_value(vull::move(error)) {}

    template <typename U>
    Result(U &ref) requires(is_ref<T> && !is_void) : m_value(vull::ref(ref)) {}
    template <typename U>
    Result(const U &ref) requires(is_ref<T> && !is_void) : m_value(vull::cref(ref)) {}

    Result(const Result &) = delete;
    Result(Result &&) = delete;
    ~Result() = default;

    Result &operator=(const Result &) = delete;
    Result &operator=(Result &&) = delete;

    explicit operator bool() const { return !is_error(); }
    bool is_error() const { return m_value.index() > 0; }

    auto disown_value();
    auto error() const;
    auto &value();
    const auto &value() const;
    Optional<T> to_optional();
};

template <typename T, typename... Es>
auto Result<T, Es...>::disown_value() {
    VULL_ASSERT(!is_error());
    return vull::move(m_value.template get<storage_t>());
}

template <typename T, typename... Es>
auto Result<T, Es...>::error() const {
    VULL_ASSERT(is_error());
    return m_value.template downcast<Es...>();
}

template <typename T, typename... Es>
auto &Result<T, Es...>::value() {
    VULL_ASSERT(!is_error());
    return vull::maybe_unwrap(m_value.template get<storage_t>());
}

template <typename T, typename... Es>
const auto &Result<T, Es...>::value() const {
    VULL_ASSERT(!is_error());
    return vull::maybe_unwrap(m_value.template get<storage_t>());
}

template <typename T, typename... Es>
Optional<T> Result<T, Es...>::to_optional() {
    return is_error() ? vull::nullopt : Optional<T>(vull::move(value()));
}

} // namespace vull
