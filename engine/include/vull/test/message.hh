#pragma once

#include <vull/support/integral.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/type_name.hh>
#include <vull/support/variant.hh>

namespace vull::test {

template <typename T>
void pretty_print(StringBuilder &sb, const T &) {
    sb.append("{}()", vull::type_name<T>());
}

template <Integral T>
void pretty_print(StringBuilder &sb, const T &value) {
    sb.append("<{}>", value);
}

inline void pretty_print(StringBuilder &sb, float value) {
    sb.append("<{}>", value);
}

inline void pretty_print(StringBuilder &sb, double value) {
    sb.append("<{}>", value);
}

class Message {
    StringBuilder m_sb;

public:
    void append_text(StringView text) { m_sb.append(text); }

    template <typename T>
    void append_description_of(const T &);

    template <typename T>
    void append_value(const T &value) {
        pretty_print(m_sb, value);
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

#define DEFINE_PRETTY_PRINTER(type_)                                                                                   \
    template <>                                                                                                        \
    void ::vull::test::pretty_print(vull::StringBuilder &sb, [[maybe_unused]] const type_ &value)

} // namespace vull::test
