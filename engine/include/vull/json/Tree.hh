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

enum class JsonError {
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

struct Value : private Variant<Null, Object, Array, String, bool, int64_t, double> {
    using Variant::has;
    using Variant::Variant;

    template <typename T>
    Result<const T &, JsonError> get() const;
    JsonResult operator[](uint32_t index) const;
    JsonResult operator[](StringView key) const;
};

struct JsonResult : public Result<const Value &, JsonError> {
    using Result<const Value &, JsonError>::Result;

    template <typename T>
    Result<const T &, JsonError> get() const;
    template <typename T>
    bool has() const;
    JsonResult operator[](uint32_t index) const;
    JsonResult operator[](StringView key) const;
};

template <typename T>
Result<const T &, JsonError> Value::get() const {
    if (!has<T>()) {
        if constexpr (is_same<T, Object>) {
            return JsonError::NotAnObject;
        } else if constexpr (is_same<T, Array>) {
            return JsonError::NotAnArray;
        } else if constexpr (is_same<T, String>) {
            return JsonError::NotAString;
        } else if constexpr (is_same<T, bool>) {
            return JsonError::NotABool;
        } else if constexpr (is_same<T, int64_t>) {
            return JsonError::NotAnInteger;
        } else if constexpr (is_same<T, double>) {
            return JsonError::NotANumber;
        } else {
            static_assert(!is_same<T, T>);
        }
    }
    return Variant::get<T>();
}

template <typename T>
Result<const T &, JsonError> JsonResult::get() const {
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
