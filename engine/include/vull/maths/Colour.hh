#pragma once

#include <vull/maths/Common.hh>
#include <vull/maths/Vec.hh>

namespace vull {

/// A class for representing a colour in linear space.
class Colour {
    Vec4f m_rgba;

    explicit Colour(const Vec4f &rgba) : m_rgba(rgba) {}

public:
    /**
     * Creates a colour from RGB(A) components in linear space.
     * @param r red
     * @param g green
     * @param b blue
     * @param a alpha
     * @return  a Colour in linear space
     * @note    alpha is passed through unmodified
     */
    static Colour from_rgb(float r, float g, float b, float a = 1.0f) { return Colour(Vec4f(r, g, b, a)); }

    /**
     * @copybrief  from_rgb(float, float, float, float)
     * @param rgba RGBA components in Vec4f form
     * @return     a Colour in linear space
     */
    static Colour from_rgb(const Vec4f &rgba) { return Colour(rgba); }

    /**
     * Creates a colour from RGB(A) components in the sRGB colour space.
     * @param r red
     * @param g green
     * @param b blue
     * @param a alpha
     * @return  a Colour in linear space
     * @note    alpha is passed through unmodified
     */
    static Colour from_srgb(float r, float g, float b, float a = 1.0f);

    /**
     * Creates a colour from HSL(A) components, assumed to be in the sRGB colour space.
     * @param h hue
     * @param s saturation
     * @param l lightness
     * @param a alpha
     * @return  a Colour in linear space
     * @note    alpha is passed through unmodified
     */
    static Colour from_hsl(float h, float s, float l, float a = 1.0f);

    /**
     * Generates a random colour.
     * @return a Colour in linear space
     */
    static Colour make_random();

    static Colour black() { return from_rgb(0.0f, 0.0f, 0.0f); }
    static Colour white() { return from_rgb(1.0f, 1.0f, 1.0f); }

    Colour() = default;

    Vec3f rgb() const { return m_rgba; }
    Vec4f rgba() const { return m_rgba; }
};

template <>
inline Colour lerp(Colour a, Colour b, float x) {
    return Colour::from_rgb(vull::lerp(a.rgba(), b.rgba(), x));
}

} // namespace vull
