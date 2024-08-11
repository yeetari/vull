#pragma once

#include <vull/json/tree.hh>
#include <vull/test/matchers.hh>
#include <vull/test/message.hh>

namespace vull::test {

template <>
struct PrettyPrinter<json::Value> {
    void operator()(StringBuilder &sb, const json::Value &value) {
        if (value.is_null()) {
            sb.append("null");
        } else if (value.has<double>()) {
            sb.append("{}", VULL_ASSUME(value.get<double>()));
        } else if (value.has<int64_t>()) {
            sb.append("{}", VULL_ASSUME(value.get<int64_t>()));
        } else if (value.has<bool>()) {
            sb.append("{}", VULL_ASSUME(value.get<bool>()));
        } else if (value.has<String>()) {
            sb.append("{}", VULL_ASSUME(value.get<String>()));
        } else if (value.has<json::Array>()) {
            const auto &vector = VULL_ASSUME(value.get<json::Array>()).data();
            PrettyPrinter<Vector<json::Value>>{}(sb, vector);
        } else if (value.has<json::Object>()) {
            const auto &object = VULL_ASSUME(value.get<json::Object>());
            if (object.empty()) {
                sb.append("empty object");
            } else {
                for (uint32_t i = 0; i < object.size(); i++) {
                    sb.append("{}=", object.keys()[i]);
                    (*this)(sb, object.values()[i]);
                    if (i != object.size() - 1) {
                        sb.append(", ");
                    }
                }
            }
        }
    }
};

namespace matchers {

template <typename ValueT, typename MatcherT>
class OfJsonValue {
    MatcherT m_matcher;

public:
    constexpr explicit OfJsonValue(const MatcherT &matcher) : m_matcher(matcher) {}

    void describe(Message &message) const {
        message.append_text("a JSON value that is ");
        m_matcher.describe(message);
    }

    void describe_mismatch(Message &message, const auto &actual) const {
        if (actual.is_error()) {
            message.append_text("an error ");
            message.append_value(actual.error());
            return;
        }
        message.append_value(actual.value());
    }

    bool matches(const auto &actual) const {
        if constexpr (is_same<ValueT, json::Null>) {
            return actual.is_null();
        } else {
            // else branch is required after the return so that this code is discarded.
            if (!actual.template has<ValueT>()) {
                return false;
            }
            return m_matcher.matches(VULL_ASSUME(actual.template get<ValueT>()));
        }
    }
};

template <typename ValueT>
constexpr auto of_json_value(const auto &matcher) {
    return OfJsonValue<ValueT, decltype(matcher)>(matcher);
}

constexpr auto of_json_null() {
    // It doesn't matter what this matcher does as it won't be called.
    auto matcher = null();
    return OfJsonValue<json::Null, decltype(matcher)>(matcher);
}

} // namespace matchers

} // namespace vull::test
