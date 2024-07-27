#include <vull/script/value.hh>

#include <vull/script/environment.hh>
#include <vull/script/vm.hh>
#include <vull/support/assert.hh>
#include <vull/support/enum.hh>
#include <vull/support/optional.hh>
#include <vull/support/span.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>

namespace vull::script {

bool Value::is_object() const {
    return vull::to_underlying(m_type) >= vull::to_underlying(Type::Symbol);
}

bool Value::is_string_object() const {
    return m_type == Type::Symbol || m_type == Type::String;
}

Optional<Object &> Value::as_object() const {
    return is_object() ? *vull::bit_cast<Object *>(m_data) : Optional<Object &>();
}

Optional<StringObject &> Value::as_string_object() const {
    return is_string_object() ? *m_data.string : Optional<StringObject &>();
}

Optional<StringView> Value::as_symbol() const {
    return m_type == Type::Symbol ? m_data.string->view() : Optional<StringView>();
}

Optional<StringView> Value::as_string() const {
    return m_type == Type::String ? m_data.string->view() : Optional<StringView>();
}

Optional<ListObject &> Value::as_list() const {
    return m_type == Type::List ? *m_data.list : Optional<ListObject &>();
}

Optional<ClosureObject &> Value::as_closure() const {
    return m_type == Type::Closure ? *m_data.closure : Optional<ClosureObject &>();
}

void Value::format_into(StringBuilder &sb) const {
    switch (m_type) {
    case Type::Null:
        sb.append("null");
        break;
    case Type::Integer:
        sb.append("{}", m_data.integer);
        break;
    case Type::Real:
        sb.append("{}", m_data.real);
        break;
    case Type::Symbol:
        sb.append("{}", m_data.string->view());
        break;
    case Type::String:
        sb.append("\"{}\"", m_data.string->view());
        break;
    case Type::List:
        sb.append('(');
        for (bool first = true; const auto &value : *m_data.list) {
            if (!first) {
                sb.append(' ');
            }
            first = false;
            value.format_into(sb);
        }
        sb.append(')');
        break;
    case Type::Closure:
        sb.append("<closure>");
        break;
    case Type::NativeFn:
        sb.append("<native fn at {h}>", vull::bit_cast<uintptr_t>(m_data.native_fn));
        break;
    default:
        vull::unreachable();
    }
}

int64_t Value::integer() const {
    VULL_ASSERT(m_type == Type::Integer);
    return m_data.integer;
}

double Value::real() const {
    VULL_ASSERT(m_type == Type::Real);
    return m_data.real;
}

StringView Value::string() const {
    VULL_ASSERT(m_type == Type::Symbol || m_type == Type::String);
    return m_data.string->view();
}

NativeFn Value::native_fn() const {
    VULL_ASSERT(m_type == Type::NativeFn);
    return m_data.native_fn;
}

Object::Object(ObjectType type) {
    m_header = static_cast<uintptr_t>(type) << 1u;
}

Optional<ListObject &> Object::as_list() {
    return type() == ObjectType::List ? static_cast<ListObject &>(*this) : Optional<ListObject &>();
}

Optional<ClosureObject &> Object::as_closure() {
    return type() == ObjectType::Closure ? static_cast<ClosureObject &>(*this) : Optional<ClosureObject &>();
}

Optional<Environment &> Object::as_environment() {
    return type() == ObjectType::Environment ? static_cast<Environment &>(*this) : Optional<Environment &>();
}

void Object::set_next_object(Object *next_object) {
    const auto address = vull::bit_cast<uintptr_t>(next_object);
    VULL_ASSERT((address & ~0xfull) == address);
    m_header &= 0xfu;
    m_header |= address;
}

void Object::set_marked(bool marked) {
    m_header &= ~1ull;
    if (marked) {
        m_header |= 1u;
    }
}

Object *Object::next_object() const {
    return vull::bit_cast<Object *>(m_header & ~0xfull);
}

ObjectType Object::type() const {
    return static_cast<ObjectType>((m_header >> 1u) & 0b111u);
}

bool Object::marked() const {
    return (m_header & 1u) != 0u;
}

Value *ListObject::begin() const {
    return vull::bit_cast<Value *>(this + 1);
}

Value *ListObject::end() const {
    return begin() + m_size;
}

Value &ListObject::at(size_t index) const {
    VULL_ASSERT(index < m_size);
    return begin()[index];
}

Value &ListObject::operator[](size_t index) const {
    VULL_ASSERT(index < m_size);
    return begin()[index];
}

Span<Value> ListObject::span() const {
    return vull::make_span(begin(), m_size);
}

StringView StringObject::view() const {
    const auto *bytes = vull::bit_cast<const char *>(this + 1);
    return {bytes, m_length};
}

Environment &ClosureObject::bind_arguments(Vm &vm, ListObject &arguments) const {
    if (arguments.empty()) {
        return *m_environment;
    }

    auto &environment = vm.make_environment(*m_environment);
    for (size_t i = 0; i < m_bindings->size(); i++) {
        environment.put_symbol(*m_bindings->at(i).as_symbol(), arguments[i]);
    }
    return environment;
}

} // namespace vull::script
