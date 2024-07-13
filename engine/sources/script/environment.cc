#include <vull/script/environment.hh>

#include <vull/container/hash_map.hh>
#include <vull/script/value.hh>
#include <vull/support/optional.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>

namespace vull::script {

Optional<Value> Environment::lookup_symbol(StringView name) const {
    auto value = m_symbol_map.get(name);
    if (value) {
        return *value;
    }
    return vull::nullopt;
}

void Environment::put_symbol(StringView name, Value value) {
    // TODO: Check for redefinition.
    m_symbol_map.set(name, value);
}

} // namespace vull::script
