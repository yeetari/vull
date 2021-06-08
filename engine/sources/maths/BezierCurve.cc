#include <vull/maths/BezierCurve.hh>

#include <vull/support/Vector.hh>

#include <glm/exponential.hpp> // IWYU pragma: keep

#include <cstdint>

namespace {

std::uint32_t binomial_coefficient(std::uint32_t n, std::uint32_t k) {
    std::uint32_t r = 1;
    for (std::uint32_t d = 1; d <= k; d++) {
        r *= n--;
        r /= d;
    }
    return r;
}

float bezier_term(std::uint32_t n, std::uint32_t k, float t, float w) {
    return static_cast<float>(binomial_coefficient(n, k)) * static_cast<float>(glm::pow(1.0f - t, n - k)) *
           static_cast<float>(glm::pow(t, k)) * w;
}

} // namespace

Vector<glm::vec3> BezierCurve::construct(const Vector<glm::vec3> &control_points, float resolution) {
    Vector<glm::vec3> curve_points;
    for (float t = 0.0f; t < 1.0f; t += resolution) { // NOLINT
        auto &curve_point = curve_points.emplace();
        for (std::uint32_t k = 0; k < control_points.size(); k++) {
            const auto &control_point = control_points[k];
            curve_point.x += bezier_term(control_points.size() - 1, k, t, control_point.x);
            curve_point.y += bezier_term(control_points.size() - 1, k, t, control_point.y);
            curve_point.z += bezier_term(control_points.size() - 1, k, t, control_point.z);
        }
    }
    return curve_points;
}
