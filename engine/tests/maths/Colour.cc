#include <vull/maths/Colour.hh>

#include <vull/maths/Epsilon.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Test.hh>

using namespace vull;

TEST_CASE(Colour, FromHsl_ZeroSaturation) {
    EXPECT(vull::fuzzy_equal(Colour::from_hsl(0.0f, 0.0f, 0.0f).rgb(), Vec3f(0.0f)));
    EXPECT(vull::fuzzy_equal(Colour::from_hsl(0.0f, 0.0f, 1.0f).rgb(), Vec3f(1.0f)));
    EXPECT(vull::fuzzy_equal(Colour::from_hsl(1.0f, 0.0f, 1.0f).rgb(), Vec3f(1.0f)));
}

TEST_CASE(Colour, FromHsl_HalfLightness) {
    EXPECT(vull::fuzzy_equal(Colour::from_hsl(0.0f, 1.0f, 0.5f).rgb(), Vec3f(1.0f, 0.0f, 0.0f)));
    EXPECT(vull::fuzzy_equal(Colour::from_hsl(120.0f / 360.0f, 1.0f, 0.5f).rgb(), Vec3f(0.0f, 1.0f, 0.0f)));
    EXPECT(vull::fuzzy_equal(Colour::from_hsl(240.0f / 360.0f, 1.0f, 0.5f).rgb(), Vec3f(0.0f, 0.0f, 1.0f)));
    EXPECT(vull::fuzzy_equal(Colour::from_hsl(1.0, 1.0f, 0.5f).rgb(), Vec3f(1.0f, 0.0f, 0.0f)));
}
