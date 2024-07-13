#pragma once

#include <vull/container/hash_map.hh>
#include <vull/container/vector.hh>
#include <vull/support/span.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh> // IWYU pragma: keep

#include <stddef.h>

namespace vull::script {

class Environment;
class Object;
class StringObject;
enum class Type;
class Value;

class Vm {
    HashMap<StringView, StringObject *> m_symbol_cache;
    Vector<UniquePtr<Environment>> m_environment_stack;
    Vector<Value> m_marked;
    Object *m_object_list{nullptr};

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

    void collect_garbage();
    Value lookup_symbol(StringView name);
    Value evaluate(Value form);
};

} // namespace vull::script
