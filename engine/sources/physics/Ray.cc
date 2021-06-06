#include <vull/physics/Ray.hh>

#include <vull/core/Entity.hh>
#include <vull/core/EntityId.hh>
#include <vull/core/Transform.hh>
#include <vull/core/World.hh>
#include <vull/physics/Collider.hh>
#include <vull/physics/JohnsonSimplexSolver.hh>
#include <vull/physics/shape/Shape.hh>
#include <vull/support/Span.hh>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/vector_relational.hpp>

#include <cstdint>
#include <limits>

namespace {

// TODO: This code is duplicated in PhysicsSystem.
glm::vec3 support_transformed(const Shape &shape, const Transform &transform, const glm::vec3 &dir) {
    const auto matrix = transform.matrix();
    return matrix * glm::vec4(shape.support_point(glm::vec4(dir, 1.0f) * matrix), 1.0f);
}

} // namespace

// NOLINTNEXTLINE
Ray::Ray(World *world, const glm::vec3 &start_point, const glm::vec3 &direction, float max_distance,
         Span<EntityId> to_ignore)
    : m_world(world), m_start_point(start_point) {
    float best_param = std::numeric_limits<float>::max();
    for (auto [entity, collider, transform] : world->view<Collider, Transform>()) {
        float hit_param = 0.0f;
        glm::vec3 hit_point(start_point);
        glm::vec3 hit_normal(0.0f);
        glm::vec3 v = hit_point -
                      support_transformed(collider->shape(), *transform, glm::vec3(1e-10f)); // small bias to avoid nan
        float v_sqrd = glm::length2(v);

        JohnsonSimplexSolver jss;
        for (int i = 0; i < 20; i++) {
            if ((v_sqrd <= 0.0000001f * jss.max_vertex_sqrd()) || jss.is_full_simplex()) {
                if (hit_param < best_param) {
                    bool ok = true;
                    for (auto ignore_id : to_ignore) {
                        if (entity.id() == ignore_id) {
                            ok = false;
                            break;
                        }
                    }
                    if (ok) {
                        best_param = hit_param;
                        if (hit_param <= max_distance) {
                            m_hit = true;
                            m_hit_distance = hit_param;
                            m_hit_entity = entity.id();
                            m_hit_point = hit_point;
                            m_hit_normal = hit_normal;
                            break;
                        }
                    }
                }
            }

            glm::vec3 p = support_transformed(collider->shape(), *transform, v);
            glm::vec3 w = hit_point - p;
            float v_dot_w = glm::dot(v, w);
            if (v_dot_w >= 0.0f) {
                float v_dot_r = glm::dot(v, direction);
                if (v_dot_r >= 0.0f) {
                    break;
                }
                hit_param = hit_param - v_dot_w / v_dot_r;
                hit_point = start_point + direction * hit_param;
                hit_normal = v;
                if (!jss.is_empty_simplex()) {
                    for (std::uint8_t j = 0; j < 4; j++) {
                        if (jss.is_simplex_point(j)) {
                            jss.set_point(j, hit_point - jss.support_point(j), jss.support_point(j));
                        }
                    }
                    jss.update_max_vertex();
                    for (std::uint8_t j = 0; j < 4; j++) {
                        if (jss.is_simplex_point(j)) {
                            jss.update_edges(j);
                        }
                    }
                    for (std::uint8_t j = 0; j < 4; j++) {
                        if (jss.is_simplex_point(j)) {
                            jss.update_determinants(j);
                        }
                    }
                }
            }
            jss.add_point(w, p);
            if (jss.reduce_simplex()) {
                v = jss.calculate_closest_point();
            } else {
                jss.calculate_backup_closest_point(v);
            }
            v_sqrd = glm::length2(v);
        }
    }
    if (!glm::all(glm::epsilonEqual(m_hit_normal, glm::vec3(0.0f), glm::vec3(glm::epsilon<float>())))) {
        m_hit_normal = glm::normalize(m_hit_normal);
    }
}

Entity Ray::hit_entity() const {
    return {m_hit_entity, m_world};
}
