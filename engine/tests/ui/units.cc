#include <vull/ui/units.hh>

#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/test.hh>

using namespace vull::test::matchers;
using namespace vull::ui;

TEST_CASE(UiUnits, LayoutUnitFloat) {
    EXPECT_THAT(LayoutUnit::from_float_pixels(0.0f).to_float(), is(epsilon_equal_to(0.0f, LayoutUnit::epsilon())));
    EXPECT_THAT(LayoutUnit::from_float_pixels(100000.0f).to_float(),
                is(epsilon_equal_to(100000.0f, LayoutUnit::epsilon())));
    EXPECT_THAT(LayoutUnit::from_float_pixels(-100000.0f).to_float(),
                is(epsilon_equal_to(-100000.0f, LayoutUnit::epsilon())));

    EXPECT_THAT(LayoutUnit::from_float_pixels(1.0f / 3.0f).to_float(),
                is(epsilon_equal_to(1.0f / 3.0f, LayoutUnit::epsilon())));
    EXPECT_THAT(LayoutUnit::from_float_pixels(-1.0f / 3.0f).to_float(),
                is(epsilon_equal_to(-1.0f / 3.0f, LayoutUnit::epsilon())));
    EXPECT_THAT(-LayoutUnit::from_float_pixels(1.0f / 3.0f).to_float(),
                is(epsilon_equal_to(-1.0f / 3.0f, LayoutUnit::epsilon())));
    EXPECT_THAT(-LayoutUnit::from_float_pixels(-1.0f / 3.0f).to_float(),
                is(epsilon_equal_to(1.0f / 3.0f, LayoutUnit::epsilon())));
    EXPECT_THAT((LayoutUnit::from_float_pixels(1.0f / 3.0f) * 3).to_float(),
                is(epsilon_equal_to(1.0f, LayoutUnit::epsilon())));
    EXPECT_THAT((LayoutUnit::from_float_pixels(-1.0f / 3.0f) * 3).to_float(),
                is(epsilon_equal_to(-1.0f, LayoutUnit::epsilon())));
    EXPECT_THAT((LayoutUnit::from_float_pixels(1.0f / 3.0f) * -3).to_float(),
                is(epsilon_equal_to(-1.0f, LayoutUnit::epsilon())));
}

TEST_CASE(UiUnits, LayoutUnitScaleBy) {
    EXPECT_THAT(LayoutUnit::from_int_pixels(10).scale_by(1.0f / 3.0f).to_float(),
                is(epsilon_equal_to(10.0f / 3.0f, LayoutUnit::epsilon())));
    EXPECT_THAT(LayoutUnit::from_int_pixels(10).scale_by(-1.0f / 3.0f).to_float(),
                is(epsilon_equal_to(-10.0f / 3.0f, LayoutUnit::epsilon())));
}

TEST_CASE(UiUnits, LayoutUnitFraction) {
    EXPECT_THAT(LayoutUnit::from_int_pixels(0).fraction(), is(equal_to(0)));
    EXPECT_THAT(LayoutUnit::from_int_pixels(1).fraction(), is(equal_to(0)));
    EXPECT_THAT((LayoutUnit::from_int_pixels(1) / 2).fraction(), is(equal_to(LayoutUnit::precision() / 2)));
    EXPECT_THAT((LayoutUnit::from_int_pixels(-1) / 2).fraction(), is(equal_to(-LayoutUnit::precision() / 2)));
    EXPECT_THAT((LayoutUnit::from_int_pixels(1) / 3).fraction(), is(equal_to(LayoutUnit::precision() / 3)));
    EXPECT_THAT((LayoutUnit::from_int_pixels(-1) / 3).fraction(), is(equal_to(-LayoutUnit::precision() / 3)));
}

TEST_CASE(UiUnits, LayoutUnitFloor) {
    EXPECT_THAT(LayoutUnit::from_int_pixels(0).floor(), is(equal_to(0)));
    EXPECT_THAT(LayoutUnit::from_int_pixels(100000).floor(), is(equal_to(100000)));
    EXPECT_THAT(LayoutUnit::from_int_pixels(-100000).floor(), is(equal_to(-100000)));

    EXPECT_THAT(LayoutUnit::from_float_pixels(0.1f).floor(), is(equal_to(0)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(0.5f).floor(), is(equal_to(0)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(0.9f).floor(), is(equal_to(0)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(1.1f).floor(), is(equal_to(1)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(-0.1f).floor(), is(equal_to(-1)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(-0.5f).floor(), is(equal_to(-1)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(-0.9f).floor(), is(equal_to(-1)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(-1.1f).floor(), is(equal_to(-2)));
}

TEST_CASE(UiUnits, LayoutUnitRound) {
    EXPECT_THAT(LayoutUnit::from_int_pixels(0).round(), is(equal_to(0)));
    EXPECT_THAT(LayoutUnit::from_int_pixels(100000).round(), is(equal_to(100000)));
    EXPECT_THAT(LayoutUnit::from_int_pixels(-100000).round(), is(equal_to(-100000)));

    EXPECT_THAT(LayoutUnit::from_float_pixels(0.1f).round(), is(equal_to(0)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(0.49f).round(), is(equal_to(0)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(0.5f).round(), is(equal_to(1)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(0.51f).round(), is(equal_to(1)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(0.9f).round(), is(equal_to(1)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(1.1f).round(), is(equal_to(1)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(1.49f).round(), is(equal_to(1)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(1.5f).round(), is(equal_to(2)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(1.51f).round(), is(equal_to(2)));

    EXPECT_THAT(LayoutUnit::from_float_pixels(-0.1f).round(), is(equal_to(0)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(-0.49f).round(), is(equal_to(0)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(-0.5f).round(), is(equal_to(-1)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(-0.51f).round(), is(equal_to(-1)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(-0.9f).round(), is(equal_to(-1)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(-1.1f).round(), is(equal_to(-1)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(-1.49f).round(), is(equal_to(-1)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(-1.5f).round(), is(equal_to(-2)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(-1.51f).round(), is(equal_to(-2)));
}

TEST_CASE(UiUnits, LayoutUnitCeil) {
    EXPECT_THAT(LayoutUnit::from_int_pixels(0).ceil(), is(equal_to(0)));
    EXPECT_THAT(LayoutUnit::from_int_pixels(100000).ceil(), is(equal_to(100000)));
    EXPECT_THAT(LayoutUnit::from_int_pixels(-100000).ceil(), is(equal_to(-100000)));

    EXPECT_THAT(LayoutUnit::from_float_pixels(0.1f).ceil(), is(equal_to(1)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(0.5f).ceil(), is(equal_to(1)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(0.9f).ceil(), is(equal_to(1)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(1.1f).ceil(), is(equal_to(2)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(-0.1f).ceil(), is(equal_to(0)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(-0.5f).ceil(), is(equal_to(0)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(-0.9f).ceil(), is(equal_to(0)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(-1.1f).ceil(), is(equal_to(-1)));
}

TEST_CASE(UiUnits, LayoutUnitToInt) {
    EXPECT_THAT(LayoutUnit::from_int_pixels(0).to_int(), is(equal_to(0)));
    EXPECT_THAT(LayoutUnit::from_int_pixels(100000).to_int(), is(equal_to(100000)));
    EXPECT_THAT(LayoutUnit::from_int_pixels(-100000).to_int(), is(equal_to(-100000)));

    EXPECT_THAT(LayoutUnit::from_float_pixels(0.1f).to_int(), is(equal_to(0)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(0.5f).to_int(), is(equal_to(0)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(0.9f).to_int(), is(equal_to(0)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(1.1f).to_int(), is(equal_to(1)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(-0.1f).to_int(), is(equal_to(0)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(-0.5f).to_int(), is(equal_to(0)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(-0.9f).to_int(), is(equal_to(0)));
    EXPECT_THAT(LayoutUnit::from_float_pixels(-1.1f).to_int(), is(equal_to(-1)));
}
