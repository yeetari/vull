#include <vull/json/Tree.hh>

#include <vull/container/Vector.hh>
#include <vull/support/Result.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Variant.hh>

namespace vull::json {

void Object::add(String &&key, Value &&value) {
    m_keys.emplace(vull::move(key));
    m_values.emplace(vull::move(value));
}

bool Object::has(StringView key) const {
    for (const auto &k : m_keys) {
        if (key == k.data()) {
            return true;
        }
    }
    return false;
}

JsonResult Object::operator[](StringView key) const {
    for (uint32_t i = 0; i < m_keys.size(); i++) {
        if (key == m_keys[i].data()) {
            return m_values[i];
        }
    }
    return JsonError::KeyNotFound;
}

JsonResult Array::operator[](uint32_t index) const {
    if (index >= m_data.size()) {
        return JsonError::OutOfBounds;
    }
    return m_data[index];
}

JsonResult Value::operator[](uint32_t index) const {
    if (!has<Array>()) {
        return JsonError::NotAnArray;
    }
    return VULL_TRY(Variant::get<Array>()[index]);
}

JsonResult Value::operator[](StringView key) const {
    if (!has<Object>()) {
        return JsonError::NotAnObject;
    }
    return VULL_TRY(Variant::get<Object>()[key]);
}

JsonResult JsonResult::operator[](uint32_t index) const {
    if (Result::is_error()) {
        return Result::error();
    }
    return VULL_TRY(Result::value()[index]);
}

JsonResult JsonResult::operator[](StringView key) const {
    if (Result::is_error()) {
        return Result::error();
    }
    return VULL_TRY(Result::value()[key]);
}

} // namespace vull::json
