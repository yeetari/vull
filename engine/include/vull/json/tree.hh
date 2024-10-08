#pragma once

#include <vull/container/vector.hh>
#include <vull/support/integral.hh>
#include <vull/support/result.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>
#include <vull/support/variant.hh>

#include <stdint.h>

namespace vull::json {

class Null {};

struct JsonResult;
struct Value;

enum class TreeError {
    KeyNotFound,
    NotAnArray,
    NotABool,
    NotAnInteger,
    NotANumber,
    NotAnObject,
    NotAString,
    OutOfBounds,
};

class Object {
    Vector<String> m_keys;
    Vector<Value> m_values;

public:
    void add(String &&key, Value &&value);
    bool has(StringView key) const;
    JsonResult operator[](StringView key) const;
    bool empty() const { return m_keys.empty(); }
    uint32_t size() const { return m_keys.size(); }

    const Vector<String> &keys() const { return m_keys; }
    const Vector<Value> &values() const { return m_values; }
};

class Array {
    Vector<Value> m_data;

public:
    void push(Value &&value) { m_data.push(vull::move(value)); }
    template <Integral I>
    JsonResult operator[](I index) const;
    bool empty() const { return m_data.empty(); }
    uint32_t size() const { return m_data.size(); }
    const Vector<Value> &data() const { return m_data; }
};

template <typename>
struct ValueHandleType;

template <ContainsType<Null, bool, int64_t, double> T>
struct ValueHandleType<T> {
    using type = T;
};

template <ContainsType<Object, Array, String> T>
struct ValueHandleType<T> {
    using type = const T &;
};

template <typename T>
using value_handle_t = ValueHandleType<T>::type;

struct Value : private Variant<Null, Object, Array, String, bool, int64_t, double> {
    using Variant::Variant;

    bool is_null() const;

    template <typename T>
    bool has() const;
    template <typename T>
    Result<value_handle_t<T>, TreeError> get() const;
    template <Integral I>
    JsonResult operator[](I index) const;
    JsonResult operator[](StringView key) const;
};

struct JsonResult : public Result<const Value &, TreeError> {
    using Result<const Value &, TreeError>::Result;

    template <typename T>
    Result<value_handle_t<T>, TreeError> get() const;
    template <typename T>
    bool has() const;
    template <Integral I>
    JsonResult operator[](I index) const;
    JsonResult operator[](StringView key) const;
};

template <Integral I>
JsonResult Array::operator[](I index) const {
    if constexpr (is_signed<I>) {
        if (index < I(0)) {
            return TreeError::OutOfBounds;
        }
    }
    if constexpr (sizeof(I) > sizeof(uint32_t)) {
        if (index > UINT32_MAX) {
            return TreeError::OutOfBounds;
        }
    }
    const auto array_index = static_cast<uint32_t>(index);
    if (array_index >= m_data.size()) {
        return TreeError::OutOfBounds;
    }
    return m_data[array_index];
}

inline bool Value::is_null() const {
    return has<Null>();
}

template <typename T>
bool Value::has() const {
    if constexpr (is_same<T, double>) {
        if (Variant::has<int64_t>()) {
            return true;
        }
    }
    return Variant::has<T>();
}

template <typename T>
Result<value_handle_t<T>, TreeError> Value::get() const {
    if constexpr (is_same<T, double>) {
        if (has<int64_t>()) {
            return static_cast<double>(Variant::get<int64_t>());
        }
    }
    if (!has<T>()) {
        if constexpr (is_same<T, Object>) {
            return TreeError::NotAnObject;
        } else if constexpr (is_same<T, Array>) {
            return TreeError::NotAnArray;
        } else if constexpr (is_same<T, String>) {
            return TreeError::NotAString;
        } else if constexpr (is_same<T, bool>) {
            return TreeError::NotABool;
        } else if constexpr (is_same<T, int64_t>) {
            return TreeError::NotAnInteger;
        } else if constexpr (is_same<T, double>) {
            return TreeError::NotANumber;
        } else {
            static_assert(!is_same<T, T>);
        }
    }
    return Variant::get<T>();
}

template <Integral I>
JsonResult Value::operator[](I index) const {
    if (!has<Array>()) {
        return TreeError::NotAnArray;
    }
    return VULL_TRY(Variant::get<Array>()[index]);
}

template <typename T>
Result<value_handle_t<T>, TreeError> JsonResult::get() const {
    if (Result::is_error()) {
        return Result::error();
    }
    return VULL_TRY(Result::value().get<T>());
}

template <typename T>
bool JsonResult::has() const {
    return !Result::is_error() && Result::value().has<T>();
}

template <Integral I>
JsonResult JsonResult::operator[](I index) const {
    if (Result::is_error()) {
        return Result::error();
    }
    return VULL_TRY(Result::value()[index]);
}

} // namespace vull::json
