#pragma once

#include <vull/support/Assert.hh>
#include <vull/support/Enum.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Variant.hh>

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

template <typename T, Enum... Es>
class [[nodiscard]] Result {
    static constexpr bool is_void = is_same<decay<T>, void>;
    using storage_t = conditional<is_void, char, conditional<is_ref<T>, RefWrapper<remove_ref<T>>, T>>;
    Variant<storage_t, Es...> m_value;

public:
    // clang-format off
    // NOLINTNEXTLINE
    Result() requires (is_void) : m_value(storage_t{}) {}
    // clang-format on

    template <ContainsType<T, Es...> U>
    Result(const U &value) : m_value(value) {}
    template <ContainsType<T, Es...> U>
    Result(U &&value) : m_value(move(value)) {}

    template <typename U>
    Result(U &ref) requires(is_ref<T> && !is_void) : m_value(vull::ref(ref)) {}
    template <typename U>
    Result(const U &ref) requires(is_ref<T> && !is_void) : m_value(vull::cref(ref)) {}

    Result(const Result &) = delete;
    Result(Result &&) = delete;
    ~Result() = default;

    Result &operator=(const Result &) = delete;
    Result &operator=(Result &&) = delete;

    bool is_error() const { return m_value.index() > 0; }
    auto disown_value() {
        VULL_ASSERT(!is_error());
        return move(m_value.template get<storage_t>());
    }
    auto error() const {
        VULL_ASSERT(is_error());
        return m_value.template downcast<Es...>();
    }
    auto &value() {
        VULL_ASSERT(!is_error());
        return vull::maybe_unwrap(m_value.template get<storage_t>());
    }
    const auto &value() const {
        VULL_ASSERT(!is_error());
        return vull::maybe_unwrap(m_value.template get<storage_t>());
    }
};

} // namespace vull
