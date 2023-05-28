#include <vull/maths/Colour.hh>

#include <vull/maths/Random.hh>
#include <vull/maths/Vec.hh>

namespace vull {

Colour Colour::from_hsl(float h, float s, float l) {
    return from_hsla(h, s, l, 1.0f);
}

static float hue_to_rgb(float p, float q, float t) {
    if (t < 0.0f) {
        t += 1.0f;
    }
    if (t > 1.0f) {
        t -= 1.0f;
    }
    if (t < 1.0f / 6.0f) {
        return p + (q - p) * 6.0f * t;
    }
    if (t < 1.0f / 2.0f) {
        return q;
    }
    if (t < 2.0f / 3.0f) {
        return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    }
    return p;
}

Colour Colour::from_hsla(float h, float s, float l, float a) {
    if (s == 0.0f) {
        return from_rgba(l, l, l, a);
    }
    float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
    float p = 2.0f * l - q;
    float r = hue_to_rgb(p, q, h + 1.0f / 3.0f);
    float g = hue_to_rgb(p, q, h);
    float b = hue_to_rgb(p, q, h - 1.0f / 3.0f);
    return from_rgba(r, g, b, a);
}

Colour Colour::from_rgb(float r, float g, float b) {
    return from_rgba(r, g, b, 1.0f);
}

Colour Colour::from_rgba(float r, float g, float b, float a) {
    return Colour(Vec4f(r, g, b, a));
}

Colour Colour::from_rgb(const Vec3f &rgb) {
    return from_rgba(Vec4f(rgb, 1.0f));
}

Colour Colour::from_rgba(const Vec4f &rgba) {
    return Colour(rgba);
}

Colour Colour::make_random() {
    float hue = vull::linear_rand(0.0f, 1.0f);
    return from_hsl(hue, 1.0f, 0.5f);
}

} // namespace vull
