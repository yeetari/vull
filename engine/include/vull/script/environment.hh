#pragma once

#include <vull/container/hash_map.hh>
#include <vull/script/value.hh>
#include <vull/support/optional.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>

namespace vull::script {

class Environment : public Object {
    Optional<Environment &> m_parent;
    HashMap<String, Value> m_symbol_map;

public:
    explicit Environment(Optional<Environment &> parent) : Object(ObjectType::Environment), m_parent(parent) {}

    Optional<Value> lookup_symbol(StringView name) const;
    void put_symbol(StringView name, Value value);

    Optional<Environment &> parent() const { return m_parent; }
    const HashMap<String, Value> &symbol_map() const { return m_symbol_map; }
};

} // namespace vull::script
