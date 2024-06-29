#include <vull/maths/colour.hh>

#include <vull/maths/vec.hh>
#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/test.hh>

using namespace vull;
using namespace vull::test::matchers;

TEST_CASE(Colour, FromHsl_ZeroSaturation) {
    EXPECT_THAT(Colour::from_hsl(0.0f, 0.0f, 0.0f).rgb(), is(close_to_zero()));
    EXPECT_THAT(Colour::from_hsl(0.0f, 0.0f, 1.0f).rgb(), is(close_to(Vec3f(1.0f))));
    EXPECT_THAT(Colour::from_hsl(1.0f, 0.0f, 1.0f).rgb(), is(close_to(Vec3f(1.0f))));
}

TEST_CASE(Colour, FromHsl_HalfLightness) {
    EXPECT_THAT(Colour::from_hsl(0.0f, 1.0f, 0.5f).rgb(), is(close_to(Vec3f(1.0f, 0.0f, 0.0f))));
    EXPECT_THAT(Colour::from_hsl(120.0f / 360.0f, 1.0f, 0.5f).rgb(), is(close_to(Vec3f(0.0f, 1.0f, 0.0f))));
    EXPECT_THAT(Colour::from_hsl(240.0f / 360.0f, 1.0f, 0.5f).rgb(), is(close_to(Vec3f(0.0f, 0.0f, 1.0f))));
    EXPECT_THAT(Colour::from_hsl(1.0f, 1.0f, 0.5f).rgb(), is(close_to(Vec3f(1.0f, 0.0f, 0.0f))));
}
