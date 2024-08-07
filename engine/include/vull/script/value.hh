#pragma once

#include <vull/support/optional.hh>
#include <vull/support/span.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>

#include <stddef.h>
#include <stdint.h>

namespace vull {

class StringBuilder;

} // namespace vull

namespace vull::script {

class ClosureObject;
class Environment;
class ListObject;
class Object;
class StringObject;
class Value;
class Vm;

enum class Type {
    // Simple types.
    Null = 0,
    Integer,
    Real,
    NativeFn,

    // Special forms.
    Def,
    Fn,
    Quote,

    // Object types.
    Symbol,
    String,
    List,
    Closure,
    Environment,
};

using NativeFn = Value (*)(Vm &, Environment &, Span<const Value>);

class Value {
    friend Vm;

private:
    union {
        int64_t integer;
        double real;
        StringObject *string;
        ListObject *list;
        ClosureObject *closure;
        NativeFn native_fn;
    } m_data{};
    Type m_type;

    explicit Value(Type type) : m_type(type) {}
    explicit Value(Type type, auto data) : m_data{vull::bit_cast<int64_t>(data)}, m_type(type) {}

public:
    static Value null() { return Value(Type::Null); }
    static Value integer(int64_t integer) { return Value(Type::Integer, integer); }
    static Value real(double real) { return Value(Type::Real, real); }
    static Value native_fn(NativeFn fn) { return Value(Type::NativeFn, fn); }

    // TODO: Only needed for bad HashMap.
    Value() : m_type(Type::Null) {}

    bool is_object() const;
    bool is_string_object() const;

    Optional<Object &> as_object() const;
    Optional<StringObject &> as_string_object() const;
    Optional<StringView> as_symbol() const;
    Optional<StringView> as_string() const;
    Optional<ListObject &> as_list() const;
    Optional<ClosureObject &> as_closure() const;

    void format_into(StringBuilder &sb) const;

    int64_t integer() const;
    double real() const;
    StringView string() const;
    NativeFn native_fn() const;
    Type type() const { return m_type; }
};

// This needs to hold for ListObject's allocation structure.
static_assert(alignof(Value) >= alignof(Value *));

enum class ObjectType : uint8_t {
    List = 0,
    String,
    Closure,
    Environment,
};

class Object {
    uintptr_t m_header{};

protected:
    explicit Object(ObjectType type);

public:
    Object(const Object &) = delete;
    Object(Object &&) = delete;
    ~Object() = default;

    Object &operator=(const Object &) = delete;
    Object &operator=(Object &&) = delete;

    Optional<ListObject &> as_list();
    Optional<ClosureObject &> as_closure();
    Optional<Environment &> as_environment();

    void set_next_object(Object *next_object);
    void set_marked(bool marked);

    Object *next_object() const;
    ObjectType type() const;
    bool marked() const;
};

class ListObject : public Object {
    friend Vm;

private:
    size_t m_size;

    ListObject(size_t size) : Object(ObjectType::List), m_size(size) {}

public:
    Value *begin() const;
    Value *end() const;

    Value &at(size_t index) const;
    Value &operator[](size_t index) const;
    Span<Value> span() const;

    bool empty() const { return m_size == 0; }
    size_t size() const { return m_size; }
};

class StringObject : public Object {
    friend Vm;

private:
    size_t m_length;

    StringObject(size_t length) : Object(ObjectType::String), m_length(length) {}

public:
    StringView view() const;
};

class ClosureObject : public Object {
    friend Vm;

private:
    Environment *m_environment;
    ListObject *m_bindings;
    Value m_body;

    ClosureObject(Environment *environment, ListObject *bindings, Value body)
        : Object(ObjectType::Closure), m_environment(environment), m_bindings(bindings), m_body(body) {}

public:
    Environment &bind_arguments(Vm &vm, ListObject &arguments) const;

    Environment &environment() const { return *m_environment; }
    ListObject &bindings() const { return *m_bindings; }
    Value body() const { return m_body; }
};

} // namespace vull::script
