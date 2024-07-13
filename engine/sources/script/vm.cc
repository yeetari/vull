#include <vull/script/vm.hh>

#include <vull/container/hash_map.hh>
#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/script/environment.hh>
#include <vull/script/value.hh>
#include <vull/support/algorithm.hh>
#include <vull/support/assert.hh>
#include <vull/support/optional.hh>
#include <vull/support/span.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>

#include <stdint.h>
#include <string.h>

namespace vull::script {

static Value add(size_t argc, Value *args) {
    int64_t integer = 0;
    double real = 0;
    bool has_real_part = false;
    for (size_t i = 0; i < argc; i++) {
        switch (args[i].type()) {
        case Type::Integer:
            integer += args[i].integer();
            break;
        case Type::Real:
            real += args[i].real();
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

static Value seq(size_t argc, Value *args) {
    return args[argc - 1];
}

Vm::Vm() {
    auto &root_environment = *m_environment_stack.emplace(new Environment);
    root_environment.put_symbol("+", Value::native_fn(&add));
    root_environment.put_symbol("seq", Value::native_fn(&seq));
}

Vm::~Vm() {
    m_environment_stack.pop();
    collect_garbage();
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

void Vm::collect_garbage() {
    // Unmark all objects.
    size_t object_count = 0;
    for (auto *object = m_object_list; object != nullptr; object = object->next_object()) {
        object->set_marked(false);
        object_count++;
    }

    Vector<ListObject &> mark_queue;

    auto inspect_value = [&](const Value &value) {
        if (auto list = value.as_list()) {
            mark_queue.push(*list);
        } else if (auto object = value.as_object()) {
            object->set_marked(true);
        }
    };

    // Iterate all environments to mark root objects.
    for (const auto &environment : m_environment_stack) {
        for (const auto &[name, value] : environment->symbol_map()) {
            inspect_value(value);
        }
    }
    for (const auto &value : m_marked) {
        inspect_value(value);
    }

    // Iteratively mark objects in lists.
    while (!mark_queue.empty()) {
        ListObject &list = mark_queue.take_last();
        if (list.marked()) {
            continue;
        }
        for (const auto &value : list) {
            inspect_value(value);
        }
        list.set_marked(true);
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
            delete[] vull::bit_cast<uint8_t *>(object);
            swept_object_count++;
        } else {
            previous = object;
        }
        object = next;
    }

    vull::debug("[vm] Swept {} out of {} objects", swept_object_count, object_count);
}

Value Vm::lookup_symbol(StringView name) {
    for (const auto &environment : vull::reverse_view(m_environment_stack)) {
        if (auto value = environment->lookup_symbol(name)) {
            return *value;
        }
    }
    vull::error("[vm] No symbol named {}", name);
    VULL_ENSURE_NOT_REACHED();
}

Value Vm::evaluate(Value form) {
    if (auto symbol = form.as_symbol()) {
        return lookup_symbol(*symbol);
    }

    auto list = form.as_list();
    if (!list) {
        return form;
    }
    if (list->empty()) {
        return Value::null();
    }

    // Handle any special forms.
    if (auto symbol = list->at(0).as_symbol()) {
        // TODO: Check argument lengths and types.
        if (*symbol == "collect-garbage") {
            collect_garbage();
            return Value::null();
        }
        if (*symbol == "define") {
            const auto name = list->at(1).as_symbol();
            m_environment_stack.last()->put_symbol(*name, evaluate(list->at(2)));
            return Value::null();
        }
        if (*symbol == "quote") {
            return list->at(1);
        }
    }

    // Evaluate list.
    auto evaluated_list_value = make_list(vull::make_span(list->begin(), list->size()));
    m_marked.push(evaluated_list_value);

    auto &evaluated_list = *evaluated_list_value.as_list();
    for (auto &value : evaluated_list) {
        value = evaluate(value);
    }
    m_marked.pop();

    const auto &op = evaluated_list[0];
    switch (op.type()) {
    case Type::NativeFn:
        return op.native_fn()(evaluated_list.size() - 1, &evaluated_list[1]);
    default:
        // Not applicable.
        VULL_ENSURE_NOT_REACHED();
    }
}

} // namespace vull::script
