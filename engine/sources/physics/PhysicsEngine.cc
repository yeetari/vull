#include <vull/physics/PhysicsEngine.hh>

#include <vull/core/Transform.hh>
#include <vull/ecs/Entity.hh>
#include <vull/ecs/World.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Mat.hh>
#include <vull/maths/Quat.hh>
#include <vull/maths/Vec.hh>
#include <vull/physics/Collider.hh>
#include <vull/physics/Contact.hh>
#include <vull/physics/Mpr.hh>
#include <vull/physics/RigidBody.hh>
#include <vull/physics/Shape.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Tuple.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>

namespace vull {

constexpr float k_fixed_timestep = 1.0f / 200.0f;
constexpr unsigned k_max_substeps = 10;

Collider::Collider(UniquePtr<Shape> &&shape) : m_shape(vull::move(shape)) {}
Collider::~Collider() = default;

// NOLINTNEXTLINE
void PhysicsEngine::sub_step(World &world, float time_step) {
    // Integrate.
    for (auto [entity, body, transform] : world.view<RigidBody, Transform>()) {
        Vec3f acceleration = body.m_force * body.m_inv_mass;
        body.m_linear_velocity += acceleration * time_step;
        transform.set_position(transform.position() + body.m_linear_velocity * time_step);

        auto mat_rotation = vull::to_mat3(transform.rotation());
        body.m_inertia_tensor_world = mat_rotation * body.m_inertia_tensor * vull::transpose(mat_rotation);
        body.m_angular_velocity += body.m_inertia_tensor_world * body.m_torque * time_step;

        Quatf delta_rotation = Quatf(body.m_angular_velocity, 0.0f) * transform.rotation() * 0.5f * time_step;
        transform.set_rotation(transform.rotation() + delta_rotation);
    }

    struct ContactInfo {
        Contact contact;
        Transform &t1;
        Transform &t2;
        RigidBody &b1;
        Optional<RigidBody &> b2;
    };

    Vector<ContactInfo> contacts;
    for (auto [e1, b1, c1, t1] : world.view<RigidBody, Collider, Transform>()) {
        for (auto [e2, c2, t2] : world.view<Collider, Transform>()) {
            if (e1 == e2) {
                continue;
            }
            if (auto contact = mpr_test(c1.shape(), t1, c2.shape(), t2)) {
                contacts.push({*contact, t1, t2, b1, e2.try_get<RigidBody>()});
            }
        }
    }

    for (auto [contact, t1, t2, b1, b2] : contacts) {
        // Get contact position in both bodies' local spacees.
        Vec3f r1 = contact.position - t1.position();
        Vec3f r2 = contact.position - t2.position();

        Vec3f relative_velocity = b1.velocity_at_point(r1) - (b2 ? b2->velocity_at_point(r2) : Vec3f());
        float velocity_projection = vull::dot(relative_velocity, contact.normal);
        if (velocity_projection > 0.0f) {
            // Bodies already separating.
            continue;
        }

        // TODO: Configurable.
        float restitution = 0.1f;
        float C = vull::max(-restitution * velocity_projection - 0.9f, 0.0f);

        Vec3f w1 = vull::cross(contact.normal, r1);
        Vec3f w2 = vull::cross(-contact.normal, r2);
        float effective_mass = b1.m_inv_mass + vull::dot(w1 * b1.m_inertia_tensor_world, w1);
        if (b2) {
            effective_mass += b2->m_inv_mass + vull::dot(w2 * b2->m_inertia_tensor_world, w2);
        }

        float impulse_magnitude = (C - velocity_projection + 0.01f) / effective_mass;
        Vec3f impulse = contact.normal * impulse_magnitude;
        b1.apply_impulse(impulse, r1);
        if (b2) {
            b2->apply_impulse(-impulse, r2);
        }

        if (contact.penetration <= 0.0f) {
            // No position correction needed.
            continue;
        }

        Vec3f prv = b1.m_pseudo_linear_velocity + vull::cross(b1.m_pseudo_angular_velocity, r1);
        if (b2) {
            prv -= b2->m_pseudo_linear_velocity + vull::cross(b2->m_pseudo_angular_velocity, r2);
        }

        float pvp = vull::dot(prv, contact.normal);
        if (pvp >= contact.penetration) {
            continue;
        }

        float pseudo_impulse_magnitude = (contact.penetration - pvp) / effective_mass;
        Vec3f pseudo_impulse = contact.normal * pseudo_impulse_magnitude;
        b1.apply_psuedo_impulse(pseudo_impulse, r1);
        if (b2) {
            b2->apply_psuedo_impulse(-pseudo_impulse, r2);
        }
    }

    for (auto [entity, body, transform] : world.view<RigidBody, Transform>()) {
        transform.set_position(transform.position() + body.m_pseudo_linear_velocity);

        Quatf delta_rotation = Quatf(body.m_pseudo_angular_velocity, 0.0f) * transform.rotation() * 0.5f;
        transform.set_rotation(transform.rotation() + delta_rotation);

        // Renormalise rotation to avoid quaternion drift - the magnitude drifting away from 1 as floating point error
        // builds up.
        transform.set_rotation(vull::normalise(transform.rotation()));

        body.m_pseudo_linear_velocity = {};
        body.m_pseudo_angular_velocity = {};
    }
}

void PhysicsEngine::step(World &world, float dt) {
    // Apply gravity.
    for (auto [entity, body] : world.view<RigidBody>()) {
        body.apply_central_force(Vec3f(0.0f, -9.81f, 0.0f) / body.m_inv_mass);
    }

    // Perform substeps.
    for (unsigned i = 0; i < k_max_substeps && dt > 0.0f; i++) {
        float time_step = vull::min(dt, k_fixed_timestep);
        sub_step(world, time_step);
        dt -= time_step;
    }

    // Clear forces.
    for (auto [entity, body] : world.view<RigidBody>()) {
        body.m_force = {};
        body.m_torque = {};
    }
}

} // namespace vull
