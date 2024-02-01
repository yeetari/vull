#include <vull/json/tree.hh>

#include <vull/container/vector.hh>
#include <vull/support/result.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>
#include <vull/support/variant.hh>

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
    return TreeError::KeyNotFound;
}

JsonResult Value::operator[](StringView key) const {
    if (!has<Object>()) {
        return TreeError::NotAnObject;
    }
    return VULL_TRY(Variant::get<Object>()[key]);
}

JsonResult JsonResult::operator[](StringView key) const {
    if (Result::is_error()) {
        return Result::error();
    }
    return VULL_TRY(Result::value()[key]);
}

} // namespace vull::json
