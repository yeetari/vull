#pragma once

#include <vull/core/BuiltinComponents.hh>
#include <vull/ecs/Component.hh>
#include <vull/physics/Shape.hh>
#include <vull/support/UniquePtr.hh>

namespace vull {

class Collider {
    VULL_DECLARE_COMPONENT(BuiltinComponents::Collider);

private:
    UniquePtr<Shape> m_shape;

public:
    [[noreturn]] static Collider deserialise(const Function<uint8_t()> &) {
        VULL_ENSURE_NOT_REACHED("TODO: Implement Collider serialisation");
    }
    [[noreturn]] static void serialise(Collider &, const Function<void(uint8_t)> &) {
        VULL_ENSURE_NOT_REACHED("TODO: Implement Collider serialisation");
    }

    Collider(UniquePtr<Shape> &&shape);
    Collider(const Collider &) = delete;
    Collider(Collider &&) = default;
    ~Collider();

    Collider &operator=(const Collider &) = delete;
    Collider &operator=(Collider &&) = default;

    const Shape &shape() const { return *m_shape; }
};

} // namespace vull