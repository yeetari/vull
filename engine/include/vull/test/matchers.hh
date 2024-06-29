#pragma once

#include <vull/maths/epsilon.hh>
#include <vull/support/type_name.hh>
#include <vull/support/variant.hh>
#include <vull/test/message.hh>

namespace vull::test {

template <typename Policy, typename T>
class Matcher : Policy {
    T m_expected;

public:
    template <typename... Args>
    Matcher(const T &expected, Args &&...args) : Policy(vull::forward<Args>(args)...), m_expected(expected) {}

    void describe(Message &message) const { Policy::describe(m_expected, message); }

    template <typename U>
    void describe_mismatch(Message &message, const U &actual) const {
        if constexpr (requires { Policy::describe_mismatch(message, m_expected, actual); }) {
            Policy::describe_mismatch(message, m_expected, actual);
            return;
        }
        message.append_text("was ");
        message.append_value(actual);
    }

    template <typename U>
    bool matches(const U &actual) const {
        return Policy::matches(m_expected, actual);
    }
};

} // namespace vull::test

namespace vull::test::matchers {

struct Is {
    template <typename T>
    void describe(const T &expected, Message &message) const {
        message.append_text("is ");
        expected.describe(message);
    }

    template <typename T, typename U>
    void describe_mismatch(Message &message, const T &expected, const U &actual) const {
        expected.describe_mismatch(message, actual);
    }

    template <typename T, typename U>
    bool matches(const T &expected, const U &actual) const {
        return expected.matches(actual);
    }
};

template <typename T>
constexpr auto is(const T &matcher) {
    return Matcher<Is, T>(matcher);
}

struct Not {
    template <typename T>
    void describe(const T &expected, Message &message) const {
        message.append_text("not ");
        expected.describe(message);
    }

    template <typename T, typename U>
    bool matches(const T &expected, const U &actual) const {
        return !expected.matches(actual);
    }
};

template <typename T>
constexpr auto not_(const T &matcher) {
    return Matcher<Not, T>(matcher);
}

struct EqualTo {
    template <typename T>
    void describe(const T &expected, Message &message) const {
        message.append_value(expected);
    }

    template <typename T>
    bool matches(const T &expected, const T &actual) const {
        return expected == actual;
    }

    template <typename T, typename... Ts>
    bool matches(const T &expected, const Variant<Ts...> &actual) const {
        return actual.template has<T>() && actual.template get<T>() == expected;
    }
};

template <typename T>
constexpr auto equal_to(const T &expected) {
    return Matcher<EqualTo, T>(expected);
}

template <typename T>
class CloseTo {
    T m_epsilon;

public:
    explicit CloseTo(T epsilon) : m_epsilon(epsilon) {}

    void describe(T expected, Message &message) const {
        message.append_text("a numeric value within ");
        message.append_value(m_epsilon);
        message.append_text(" of ");
        message.append_value(expected);
    }

    void describe_mismatch(Message &message, T expected, T actual) const {
        message.append_text("was ");
        message.append_value(actual);
        message.append_text(" which differs by ");
        message.append_value(vull::abs(actual - expected));
    }

    bool matches(T expected, T actual) const { return vull::epsilon_equal(expected, actual, m_epsilon); }
};

template <typename T>
constexpr auto close_to(T expected, T epsilon) {
    return Matcher<CloseTo<T>, T>(expected, epsilon);
}

template <typename T>
struct OfType {
    void describe(const auto &, Message &message) const {
        message.append_text("of type ");
        message.append_text(vull::type_name<T>());
    }

    bool matches(const auto &, const type_identity<T> &) const { return true; }

    template <typename U>
    bool matches(const auto &, const U &) const {
        return false;
    }

    template <typename... Ts>
    bool matches(const auto &, const Variant<Ts...> &actual) const {
        return actual.template has<T>();
    }
};

template <typename T>
constexpr auto of_type() {
    return Matcher<OfType<T>, int>({});
}

} // namespace vull::test::matchers
