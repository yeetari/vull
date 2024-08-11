#pragma once

#include <vull/maths/epsilon.hh>
#include <vull/support/optional.hh>
#include <vull/support/type_name.hh>
#include <vull/support/variant.hh>
#include <vull/test/message.hh>

// NOLINTBEGIN(readability-convert-member-functions-to-static)

namespace vull::test::matchers {

template <typename T>
class Is {
    T m_matcher;

public:
    explicit Is(const T &matcher) : m_matcher(matcher) {}

    void describe(Message &message) const {
        message.append_text("is ");
        m_matcher.describe(message);
    }

    void describe_mismatch(Message &message, const auto &actual) const { m_matcher.describe_mismatch(message, actual); }

    bool matches(const auto &actual) const { return m_matcher.matches(actual); }
};

constexpr auto is(const auto &matcher) {
    return Is(matcher);
}

template <typename T>
class Not {
    T m_matcher;

public:
    explicit Not(const T &matcher) : m_matcher(matcher) {}

    void describe(Message &message) const {
        message.append_text("not ");
        m_matcher.describe(message);
    }

    void describe_mismatch(Message &message, const auto &actual) const { m_matcher.describe_mismatch(message, actual); }

    bool matches(const auto &actual) const { return !m_matcher.matches(actual); }
};

constexpr auto not_(const auto &matcher) {
    return Not(matcher);
}

template <typename T>
class EqualTo {
    T m_expected;

public:
    explicit EqualTo(const T &expected) : m_expected(expected) {}

    void describe(Message &message) const {
        message.append_text("equal to ");
        message.append_value(m_expected);
    }

    void describe_mismatch(Message &message, const auto &actual) const {
        message.append_text("was ");
        message.append_value(actual);
    }

    bool matches(const T &actual) const { return actual == m_expected; }

    template <typename U>
    bool matches(Optional<U> actual) const {
        return actual && *actual == m_expected;
    }

    template <typename... Ts>
    bool matches(const Variant<Ts...> &actual) const {
        return actual.template has<T>() && actual.template get<T>() == m_expected;
    }
};

constexpr auto equal_to(const auto &expected) {
    return EqualTo(expected);
}

template <typename T>
class EpsilonEqualTo {
    T m_expected;
    T m_epsilon;

public:
    EpsilonEqualTo(const T &expected, const T &epsilon) : m_expected(expected), m_epsilon(epsilon) {}

    void describe(Message &message) const {
        message.append_text("a numeric value within ");
        message.append_value(m_epsilon);
        message.append_text(" of ");
        message.append_value(m_expected);
    }

    void describe_mismatch(Message &message, const T &actual) const {
        message.append_text("was ");
        message.append_value(actual);
        message.append_text(" which differs by ");
        message.append_value(vull::abs(actual - m_expected));
    }

    bool matches(const T &actual) const { return vull::epsilon_equal(m_expected, actual, m_epsilon); }
};

template <typename T>
constexpr auto epsilon_equal_to(const T &expected, const T &epsilon) {
    return EpsilonEqualTo(expected, epsilon);
}

template <typename T>
class CloseTo {
    T m_expected;

public:
    explicit CloseTo(const T &expected) : m_expected(expected) {}

    void describe(Message &message) const {
        message.append_text("a numeric value close to ");
        message.append_value(m_expected);
    }

    void describe_mismatch(Message &message, const T &actual) const {
        message.append_text("was ");
        message.append_value(actual);
        message.append_text(" which differs by ");
        message.append_value(vull::abs(actual - m_expected));
    }

    bool matches(const T &actual) const { return vull::fuzzy_equal(m_expected, actual); }
};

constexpr auto close_to(const auto &expected) {
    return CloseTo(expected);
}

class CloseToZero {
public:
    void describe(Message &message) const { message.append_text("a numeric value close to zero"); }

    void describe_mismatch(Message &message, const auto &actual) const {
        message.append_text("was ");
        message.append_value(actual);
    }

    bool matches(const auto &actual) const { return vull::fuzzy_zero(actual); }
};

constexpr auto close_to_zero() {
    return CloseToZero();
}

template <typename T>
class OfType {
public:
    void describe(Message &message) const {
        message.append_text("of type ");
        message.append_text(vull::type_name<T>());
    }

    void describe_mismatch(Message &message, const auto &actual) const {
        message.append_text("was ");
        message.append_value(actual);
        message.append_text(" (");
        message.append_text(vull::type_name<vull::decay<decltype(actual)>>());
        message.append_text(")");
    }

    bool matches(const type_identity<T> &) const { return true; }

    template <typename U>
    bool matches(const U &) const {
        return false;
    }

    template <typename... Ts>
    bool matches(const Variant<Ts...> &actual) const {
        return actual.template has<T>();
    }
};

template <typename T>
constexpr auto of_type() {
    return OfType<T>();
}

template <typename T>
class Containing {
    T m_value;

public:
    explicit Containing(const T &value) : m_value(value) {}

    void describe(Message &message) const {
        message.append_text("a collection containing ");
        message.append_value(m_value);
    }

    void describe_mismatch(Message &message, const auto &collection) const {
        if (collection.contains(m_value)) {
            message.append_text("collection contains ");
        } else {
            message.append_text("collection doesn't contain ");
        }
        message.append_value(m_value);
    }

    bool matches(const auto &collection) const { return collection.contains(m_value); }
};

constexpr auto containing(auto value) {
    return Containing(value);
}

class Null {
public:
    void describe(Message &message) const { message.append_text("null"); }

    void describe_mismatch(Message &message, const auto &actual) const {
        message.append_text("was ");
        message.append_value(actual);
    }

    template <typename T>
    bool matches(T *pointer) const {
        return pointer == nullptr;
    }

    template <typename T>
    bool matches(Optional<T> optional) const {
        return !optional.has_value();
    }
};

constexpr auto null() {
    return Null();
}

class Empty {
public:
    void describe(Message &message) const { message.append_text("empty"); }

    void describe_mismatch(Message &message, const auto &actual) const {
        message.append_text("was ");
        message.append_value(actual);
    }

    bool matches(const auto &collection) const { return collection.empty(); }
};

constexpr auto empty() {
    return Empty();
}

template <typename T>
class Success {
    // TODO: Make matcher optional, and if absent, only check for no error?
    T m_matcher;

public:
    explicit Success(const T &matcher) : m_matcher(matcher) {}

    void describe(Message &message) const {
        message.append_text("a successful result that is ");
        m_matcher.describe(message);
    }

    void describe_mismatch(Message &message, const auto &actual) const {
        if (actual.is_error()) {
            message.append_text("was error ");
            message.append_value(actual.error());
        } else {
            message.append_text("was ");
            m_matcher.describe_mismatch(message, actual);
        }
    }

    bool matches(const auto &actual) const { return !actual.is_error() && m_matcher.matches(actual.value()); }
};

constexpr auto success(const auto &matcher) {
    return Success(matcher);
}

} // namespace vull::test::matchers

// NOLINTEND(readability-convert-member-functions-to-static)
