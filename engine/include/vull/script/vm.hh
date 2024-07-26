#pragma once

#include <vull/container/hash_map.hh>
#include <vull/container/vector.hh>
#include <vull/script/environment.hh>
#include <vull/support/optional.hh>
#include <vull/support/span.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh> // IWYU pragma: keep

#include <stddef.h>

namespace vull::script {

class Object;
class StringObject;
enum class Type;
class Value;

class Vm {
    HashMap<StringView, StringObject *> m_symbol_cache;
    Vector<Object &> m_marked;
    Object *m_object_list{nullptr};
    Environment m_root_environment;

    template <typename T, typename... Args>
    T *allocate_object(size_t data_size, Args &&...args);
    Value make_stringish(Type type, StringView value);

public:
    Vm();
    Vm(const Vm &) = delete;
    Vm(Vm &&) = delete;
    ~Vm();

    Vm &operator=(const Vm &) = delete;
    Vm &operator=(Vm &&) = delete;

    Value make_symbol(StringView name);
    Value make_string(StringView value);
    Value make_list(Span<const Value> elements);

    void collect_garbage(Optional<Environment &> current_environment);
    Value evaluate(Value form);
    Value evaluate(Value form, Environment &environment);
};

} // namespace vull::script
