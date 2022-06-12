#include <vull/physics/Mpr.hh>

#include <vull/core/Transform.hh>
#include <vull/maths/Epsilon.hh>
#include <vull/maths/Quat.hh>
#include <vull/maths/Vec.hh>
#include <vull/physics/Contact.hh>
#include <vull/physics/Shape.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Utility.hh>

namespace vull {
namespace {

struct SupportPoint : Vec3f {
    Vec3f p1;
    Vec3f p2;
};

class Context {
    const Shape &m_s1;
    const Shape &m_s2;
    const Transform &m_t1;
    const Transform &m_t2;

public:
    Context(const Shape &s1, const Shape &s2, const Transform &t1, const Transform &t2)
        : m_s1(s1), m_s2(s2), m_t1(t1), m_t2(t2) {}

    SupportPoint support_point(const Vec3f &direction) const;
    Optional<Contact> run() const;
};

// Computes the support point in the given direction for the Minkowski difference of the two shapes.
SupportPoint Context::support_point(const Vec3f &direction) const {
    Vec3f p1 = m_t1 * m_s1.furthest_point(vull::rotate(vull::conjugate(m_t1.rotation()), -direction));
    Vec3f p2 = m_t2 * m_s2.furthest_point(vull::rotate(vull::conjugate(m_t2.rotation()), direction));
    return {p2 - p1, p1, p2};
}

Vec3f compute_contact_position(const SupportPoint &v0, const SupportPoint &v1, const SupportPoint &v2,
                               const SupportPoint &v3, const Vec3f &normal) {
    float b0 = vull::dot(vull::cross(v1, v2), v3);
    float b1 = vull::dot(vull::cross(v3, v2), v0);
    float b2 = vull::dot(vull::cross(v0, v1), v3);
    float b3 = vull::dot(vull::cross(v2, v1), v0);
    float sum = b0 + b1 + b2 + b3;
    if (sum <= 0.0f) {
        b0 = 0;
        b1 = vull::dot(vull::cross(v2, v3), normal);
        b2 = vull::dot(vull::cross(v3, v1), normal);
        b3 = vull::dot(vull::cross(v1, v2), normal);
        sum = b1 + b2 + b3;
    }
    float inv = 1.0f / sum;
    Vec3f position;
    position = v0.p1 * b0;
    position += v1.p1 * b1;
    position += v2.p1 * b2;
    position += v3.p1 * b3;
    position += v0.p2 * b0;
    position += v1.p2 * b1;
    position += v2.p2 * b2;
    position += v3.p2 * b3;
    position *= inv * 0.5f;
    return position;
}

// Support point always refers to a support point on the corresponding Minkowski difference between the two shapes.
// Vertex refers to a vertex of a constructed simplex (which may not necessarily be a vertex on the MD).
Optional<Contact> Context::run() const {
    // Phase 1: Portal Discovery

    // Find an interior point (v0).
    auto md_center = m_t2.position() - m_t1.position();
    if (vull::fuzzy_zero(md_center)) {
        // Minkowski difference center overlapping the origin is a hit, albeit a degenerate one, so we can use any
        // normal here.
        return Contact{
            .position = m_t1.position(),
            .normal = {0.0f, 1.0f, 0.0f},
            .penetration = 0.0f,
        };
    }

    // Find support in the direction of the origin ray.
    auto direction = -md_center;
    auto v1 = support_point(direction);
    if (vull::dot(v1, direction) <= 0.0f) {
        // Point doesn't pass the origin boundary - the constructed simplex could not possibly contain the origin.
        return {};
    }

    // Find support perpendicular to the plane containing the origin, interior point and first support.
    direction = vull::cross(v1, md_center);
    if (vull::fuzzy_zero(direction)) {
        // v0, v1 and origin colinear (and origin inside v1 support plane) == > hit
        return Contact{
            .position = (v1.p1 + v1.p2) * 0.5f,
            .normal = vull::normalise(v1 - md_center),
            .penetration = vull::dot(v1, direction),
        };
    }
    auto v2 = support_point(direction);
    if (vull::dot(v2, direction) <= 0.0f) {
        return {};
    }

    direction = vull::cross(v1 - md_center, v2 - md_center);
    if (vull::dot(md_center, direction) > 0.0f) {
        // If origin is on the negative side of the plane, reverse the direction.
        vull::swap(v1, v2);
        direction = -direction;
    }

    while (true) {
        auto v3 = support_point(direction);
        if (vull::dot(v3, direction) <= 0.0f) {
            // Origin outside v3 support plane.
            return {};
        }

        // If origin is outside (v1, v0, v3), then portal is invalid -- eliminate v2 and find new support outside face
        if (vull::dot(vull::cross(v1, v3), md_center) < 0.0f) {
            v2 = v3;
            direction = vull::cross(v1 - md_center, v3 - md_center);
            continue;
        }

        // If origin is outside (v3,v0,v2), then portal is invalid -- eliminate v1 and find new support outside face
        if (vull::dot(vull::cross(v3, v2), md_center) < 0.0f) {
            v1 = v3;
            direction = vull::cross(v3 - md_center, v2 - md_center);
            continue;
        }

        // Phase 2: Portal Refinement
        bool hit = false;
        while (true) {
            direction = vull::normalise(vull::cross(v2 - v1, v3 - v1));
            if (vull::dot(v1, direction) >= 0.0f) {
                // Origin inside the portal is a hit, but we must continue iterating to find the contact information.
                hit = true;
            }

            // If the origin is outside the support plane or the boundary is thin enough, we have a miss
            auto v4 = support_point(direction);
            float penetration = vull::dot(v4, direction);
            if (penetration <= 0.0f || vull::dot(v4 - v3, direction) <= 1e-4f) {
                // We have finished iterating.
                if (!hit) {
                    return {};
                }
                SupportPoint v0{md_center, m_t1.position(), m_t2.position()};
                return Contact{
                    .position = compute_contact_position(v0, v1, v2, v3, direction),
                    .normal = direction,
                    .penetration = penetration,
                };
            }

            // Test origin against the three planes that separate the new portal candidates: (v1,v4,v0) (v2,v4,v0)
            // (v3,v4,v0) Note:  We're taking advantage of the triple product identities here as an optimization
            //        (v1 % v4) * v0 == v1 * (v4 % v0)    > 0 if origin inside (v1, v4, v0)
            //        (v2 % v4) * v0 == v2 * (v4 % v0)    > 0 if origin inside (v2, v4, v0)
            //        (v3 % v4) * v0 == v3 * (v4 % v0)    > 0 if origin inside (v3, v4, v0)
            auto cross = vull::cross(v4, md_center);
            if (vull::dot(v1, cross) > 0.0f) {
                if (vull::dot(v2, cross) > 0.0f) {
                    v1 = v4; // Inside v1 & inside v2 ==> eliminate v1
                } else {
                    v3 = v4; // Inside v1 & outside v2 ==> eliminate v3
                }
            } else {
                if (vull::dot(v3, cross) > 0.0f) {
                    v2 = v4; // Outside v1 & inside v3 ==> eliminate v2
                } else {
                    v1 = v4; // Outside v1 & outside v3 ==> eliminate v1
                }
            }
        }
    }
}

} // namespace

Optional<Contact> mpr_test(const Shape &s1, const Transform &t1, const Shape &s2, const Transform &t2) {
    return Context(s1, s2, t1, t2).run();
}

} // namespace vull
