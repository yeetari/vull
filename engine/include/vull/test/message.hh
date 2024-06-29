#pragma once

#include <vull/container/vector.hh>
#include <vull/support/integral.hh>
#include <vull/support/optional.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/type_name.hh>
#include <vull/support/variant.hh>

namespace vull::test {

template <typename T>
concept Numeric = is_integral<T> || is_same<T, float> || is_same<T, double>;

template <typename T>
concept StringLike = is_same<T, const char *> || is_same<T, String> || is_same<T, StringView>;

template <typename T>
struct PrettyPrinter {
    void operator()(StringBuilder &sb, const T &) { sb.append("{}()", vull::type_name<T>()); }
};

template <typename T>
struct PrettyPrinter<T *> {
    void operator()(StringBuilder &sb, T *value) {
        if (value == nullptr) {
            sb.append("null");
        } else {
            sb.append("{}({h})", vull::type_name<T *>(), vull::bit_cast<uintptr_t>(value));
        }
    }
};

template <>
struct PrettyPrinter<bool> {
    void operator()(StringBuilder &sb, bool value) { sb.append(value ? "<true>" : "<false>"); }
};

template <Numeric T>
struct PrettyPrinter<T> {
    void operator()(StringBuilder &sb, T value) { sb.append("<{}>", value); }
};

template <typename T>
struct PrettyPrinter<Optional<T>> {
    void operator()(StringBuilder &sb, auto optional) {
        if (optional) {
            PrettyPrinter<T>{}(sb, *optional);
        } else {
            sb.append("empty {}", vull::type_name<Optional<T>>());
        }
    }
};

template <StringLike T>
struct PrettyPrinter<T> {
    void operator()(StringBuilder &sb, StringView value) { sb.append("\"{}\"", value); }
};

template <typename T>
struct PrettyPrinter<Vector<T>> {
    void operator()(StringBuilder &sb, const Vector<T> &vector) {
        // TODO: Print actual vector contents with heuristic.
        sb.append("{}(size: {})", vull::type_name<Vector<T>>(), vector.size());
    }
};

class Message {
    StringBuilder m_sb;

public:
    void append_text(StringView text) { m_sb.append(text); }

    template <typename T>
    void append_description_of(const T &);

    template <typename T>
    void append_value(const T &value) {
        PrettyPrinter<T>{}(m_sb, value);
    }

    template <typename T, typename... Ts>
    void append_variant(const Variant<Ts...> &variant) {
        if (variant.template has<T>()) {
            append_value(variant.template get<T>());
        }
    }

    template <typename... Ts>
    void append_value(const Variant<Ts...> &variant) {
        (append_variant<Ts, Ts...>(variant), ...);
    }

    String build() { return m_sb.build(); }
};

} // namespace vull::test
