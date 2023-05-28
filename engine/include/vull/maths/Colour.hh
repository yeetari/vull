#pragma once

#include <vull/maths/Common.hh>
#include <vull/maths/Vec.hh>

namespace vull {

class Colour {
    Vec4f m_rgba;

    explicit Colour(const Vec4f &rgba) : m_rgba(rgba) {}

public:
    static Colour from_hsl(float h, float s, float l);
    static Colour from_hsla(float h, float s, float l, float a);
    static Colour from_rgb(float r, float g, float b);
    static Colour from_rgba(float r, float g, float b, float a);
    static Colour from_rgb(const Vec3f &rgb);
    static Colour from_rgba(const Vec4f &rgba);
    static Colour make_random();

    static Colour black() { return from_rgb(0.0f, 0.0f, 0.0f); }
    static Colour white() { return from_rgb(1.0f, 1.0f, 1.0f); }

    Colour() = default;

    Vec3f to_rgb() const { return m_rgba; }
    Vec4f to_rgba() const { return m_rgba; }
};

template <>
inline Colour lerp(Colour a, Colour b, float x) {
    return Colour::from_rgba(vull::lerp(a.to_rgba(), b.to_rgba(), x));
}

} // namespace vull
