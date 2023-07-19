#pragma once

#include <vull/container/Vector.hh>
#include <vull/support/Result.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Variant.hh>

#include <stddef.h>
#include <stdint.h>

namespace vull::json {

class Null {};

using String = vull::String;

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
    size_t size() const { return m_keys.size(); }
};

class Array {
    Vector<Value> m_data;

public:
    void push(Value &&value) { m_data.push(vull::move(value)); }
    JsonResult operator[](uint32_t index) const;
    bool empty() const { return m_data.empty(); }
    size_t size() const { return m_data.size(); }
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
    using Variant::has;
    using Variant::Variant;

    template <typename T>
    Result<value_handle_t<T>, TreeError> get() const;
    JsonResult operator[](uint32_t index) const;
    JsonResult operator[](StringView key) const;
};

struct JsonResult : public Result<const Value &, TreeError> {
    using Result<const Value &, TreeError>::Result;

    template <typename T>
    Result<value_handle_t<T>, TreeError> get() const;
    template <typename T>
    bool has() const;
    JsonResult operator[](uint32_t index) const;
    JsonResult operator[](StringView key) const;
};

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

} // namespace vull::json
