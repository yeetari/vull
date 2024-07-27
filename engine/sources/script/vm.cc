#include <vull/script/vm.hh>

#include <vull/container/hash_map.hh>
#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/script/environment.hh>
#include <vull/script/value.hh>
#include <vull/support/assert.hh>
#include <vull/support/optional.hh>
#include <vull/support/span.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>

#include <stdint.h>
#include <string.h>

namespace vull::script {

static Value native_add(Vm &, Environment &, Span<const Value> args) {
    int64_t integer = 0;
    double real = 0;
    bool has_real_part = false;
    for (const auto &value : args) {
        switch (value.type()) {
        case Type::Integer:
            integer += value.integer();
            break;
        case Type::Real:
            real += value.real();
            has_real_part = true;
            break;
        default:
            VULL_ENSURE_NOT_REACHED();
        }
    }

    if (!has_real_part) {
        return Value::integer(integer);
    }
    return Value::real(real + static_cast<double>(integer));
}

static Value native_collect_garbage(Vm &vm, Environment &environment, Span<const Value>) {
    vm.collect_garbage(environment);
    return Value::null();
}

static Value native_seq(Vm &, Environment &, Span<const Value> args) {
    return args.end()[-1];
}

Vm::Vm() : m_root_environment(vull::nullopt) {
    // Special forms.
    m_root_environment.put_symbol("def!", Value(Type::Def));
    m_root_environment.put_symbol("fn", Value(Type::Fn));
    m_root_environment.put_symbol("quote", Value(Type::Quote));

    // Functions backed by native code.
    m_root_environment.put_symbol("+", Value::native_fn(&native_add));
    m_root_environment.put_symbol("collect-garbage", Value::native_fn(&native_collect_garbage));
    m_root_environment.put_symbol("seq", Value::native_fn(&native_seq));
}

Vm::~Vm() {
    collect_garbage(vull::nullopt);
    VULL_ASSERT(m_object_list == nullptr);
}

template <typename T, typename... Args>
T *Vm::allocate_object(size_t data_size, Args &&...args) {
    auto *bytes = new uint8_t[sizeof(T) + data_size];
    auto *object = new (bytes) T(vull::forward<Args>(args)...);
    object->set_next_object(m_object_list);
    m_object_list = object;
    return object;
}

Value Vm::make_stringish(Type type, StringView value) {
    auto *object = allocate_object<StringObject>(value.length(), value.length());
    memcpy(vull::bit_cast<void *>(object + 1), value.data(), value.length());
    return Value(type, object);
}

Value Vm::make_symbol(StringView name) {
    if (auto object = m_symbol_cache.get(name)) {
        return Value(Type::Symbol, *object);
    }
    auto value = make_stringish(Type::Symbol, name);
    auto *object = value.as_string_object().ptr();
    m_symbol_cache.set(object->view(), object);
    return value;
}

Value Vm::make_string(StringView value) {
    return make_stringish(Type::String, value);
}

Value Vm::make_list(Span<const Value> elements) {
    auto *object = allocate_object<ListObject>(elements.size_bytes(), elements.size());
    memcpy(vull::bit_cast<void *>(object + 1), elements.data(), elements.size_bytes());
    return Value(Type::List, object);
}

Value Vm::make_closure(Environment &environment, Value bindings, Value body) {
    auto *object = allocate_object<ClosureObject>(0, &environment, bindings.as_list().ptr(), body);
    return Value(Type::Closure, object);
}

Environment &Vm::make_environment(Environment &parent) {
    return *allocate_object<Environment>(0, Optional<Environment &>(parent));
}

void Vm::collect_garbage(Optional<Environment &> current_environment) {
    // Unmark all objects.
    size_t object_count = 0;
    for (auto *object = m_object_list; object != nullptr; object = object->next_object()) {
        object->set_marked(false);
        object_count++;
    }

    // Mark roots.
    Vector<Object &> mark_queue;
    mark_queue.extend(m_marked);
    if (current_environment) {
        mark_queue.push(*current_environment);
    }

    auto mark_value = [&](Value value) {
        if (auto object = value.as_object()) {
            mark_queue.push(*object);
        }
    };

    // Iteratively mark through the object graph.
    while (!mark_queue.empty()) {
        Object &object = mark_queue.take_last();
        if (object.marked()) {
            continue;
        }

        if (auto list = object.as_list()) {
            for (const auto &list_item : *list) {
                mark_value(list_item);
            }
        } else if (auto closure = object.as_closure()) {
            mark_queue.push(closure->environment());
            mark_queue.push(closure->bindings());
            mark_value(closure->body());
        } else if (auto environment = object.as_environment()) {
            if (auto parent = environment->parent()) {
                mark_queue.push(*parent);
            }
            for (const auto &[name, value] : environment->symbol_map()) {
                mark_value(value);
            }
        }
        object.set_marked(true);
    }

    size_t swept_object_count = 0;

    // Sweep unmarked objects.
    Object *previous = nullptr;
    for (auto *object = m_object_list; object != nullptr;) {
        auto *next = object->next_object();
        if (!object->marked()) {
            if (previous != nullptr) {
                previous->set_next_object(next);
            }
            if (m_object_list == object) {
                m_object_list = next;
            }
            if (auto environment = object->as_environment()) {
                environment->~Environment();
            }
            delete[] vull::bit_cast<uint8_t *>(object);
            swept_object_count++;
        } else {
            previous = object;
        }
        object = next;
    }

    vull::debug("[vm] Swept {} out of {} objects", swept_object_count, object_count);
}

Value Vm::evaluate(Value form) {
    Environment environment{Optional<Environment &>(m_root_environment)};
    return evaluate(form, environment);
}

Value Vm::evaluate(Value form, Environment &environment) {
    if (auto symbol = form.as_symbol()) {
        if (auto value = environment.lookup_symbol(*symbol)) {
            return *value;
        }
        vull::error("[vm] No symbol named {}", *symbol);
        VULL_ENSURE_NOT_REACHED();
    }

    auto list = form.as_list();
    if (!list) {
        return form;
    }
    if (list->empty()) {
        return Value::null();
    }

    // Evaluate operation name.
    const auto op = evaluate(list->at(0), environment);

    // TODO: Check argument list length and types.
    switch (op.type()) {
    case Type::Def: {
        const auto name = list->at(1).as_symbol();
        const auto value = evaluate(list->at(2), environment);
        environment.put_symbol(*name, value);
        return Value::null();
    }
    case Type::Fn:
        return make_closure(environment, list->at(1), list->at(2));
    case Type::Quote:
        return list->at(1);
    case Type::Closure:
    case Type::NativeFn: {
        // Evaluate all arguments.
        auto argument_list_value = make_list(list->span().subspan(1));
        auto &argument_list = *argument_list_value.as_list();
        m_marked.push(argument_list);
        for (auto &value : argument_list) {
            value = evaluate(value, environment);
        }
        m_marked.pop();

        if (auto closure = op.as_closure()) {
            auto &closure_environment = closure->bind_arguments(*this, argument_list);
            return evaluate(closure->body(), closure_environment);
        }
        return op.native_fn()(*this, environment, argument_list.span());
    }
    default:
        // TODO: Not applicable.
        VULL_ENSURE_NOT_REACHED();
    }
}

} // namespace vull::script
