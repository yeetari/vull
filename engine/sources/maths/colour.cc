#include <vull/maths/colour.hh>

#include <vull/maths/random.hh>
#include <vull/maths/relational.hh>
#include <vull/maths/vec.hh>

namespace vull {
namespace {

// See the Khronos Data Format Specification v1.3.1 section 13.3.1
Vec3f srgb_to_linear(Vec3f srgb) {
    auto r1 = srgb / 12.92f;
    auto r2 = vull::pow((srgb + 0.055f) / 1.055f, Vec3f(2.4f));
    return vull::select(r1, r2, vull::greater_than(srgb, Vec3f(0.04045f)));
}

// See the Khronos Data Format Specification v1.3.1 section 13.3.2
[[maybe_unused]] Vec3f linear_to_srgb(Vec3f linear) {
    auto r1 = linear * 12.92f;
    auto r2 = vull::pow(linear, Vec3f(1.0f / 2.4f)) * 1.055f - 0.055f;
    return vull::select(r1, r2, vull::greater_than(linear, Vec3f(0.0031308f)));
}

float hue_to_rgb(float p, float q, float t) {
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

} // namespace

Colour Colour::from_srgb(float r, float g, float b, float a) {
    return Colour(Vec4f(srgb_to_linear(Vec3f(r, g, b)), a));
}

Colour Colour::from_hsl(float h, float s, float l, float a) {
    if (s == 0.0f) {
        return from_rgb(l, l, l, a);
    }
    float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
    float p = 2.0f * l - q;
    float r = hue_to_rgb(p, q, h + 1.0f / 3.0f);
    float g = hue_to_rgb(p, q, h);
    float b = hue_to_rgb(p, q, h - 1.0f / 3.0f);
    return from_srgb(r, g, b, a);
}

Colour Colour::make_random() {
    float hue = vull::linear_rand(0.0f, 1.0f);
    return from_hsl(hue, 1.0f, 0.5f);
}

} // namespace vull
