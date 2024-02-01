#include <vull/ui/units.hh>

#include <vull/maths/epsilon.hh>
#include <vull/support/test.hh>

using namespace vull::ui;

static bool unit_equal(LayoutUnit actual, float expected) {
    return vull::epsilon_equal(actual.to_float(), expected, LayoutUnit::epsilon());
}

TEST_CASE(UiUnits, LayoutUnitFloat) {
    EXPECT(unit_equal(LayoutUnit::from_float_pixels(0.0f), 0.0f));
    EXPECT(unit_equal(LayoutUnit::from_float_pixels(100000.0f), 100000.0f));
    EXPECT(unit_equal(LayoutUnit::from_float_pixels(-100000.0f), -100000.0f));

    EXPECT(unit_equal(LayoutUnit::from_float_pixels(1.0f / 3.0f), 1.0f / 3.0f));
    EXPECT(unit_equal(LayoutUnit::from_float_pixels(-1.0f / 3.0f), -1.0f / 3.0f));
    EXPECT(unit_equal(-LayoutUnit::from_float_pixels(1.0f / 3.0f), -1.0f / 3.0f));
    EXPECT(unit_equal(-LayoutUnit::from_float_pixels(-1.0f / 3.0f), 1.0f / 3.0f));
    EXPECT(unit_equal(LayoutUnit::from_float_pixels(1.0f / 3.0f) * 3, 1.0f));
    EXPECT(unit_equal(LayoutUnit::from_float_pixels(-1.0f / 3.0f) * 3, -1.0f));
    EXPECT(unit_equal(LayoutUnit::from_float_pixels(1.0f / 3.0f) * -3, -1.0f));
}

TEST_CASE(UiUnits, LayoutUnitScaleBy) {
    EXPECT(unit_equal(LayoutUnit::from_int_pixels(10).scale_by(1.0f / 3.0f), 10.0f / 3.0f));
    EXPECT(unit_equal(LayoutUnit::from_int_pixels(10).scale_by(-1.0f / 3.0f), -10.0f / 3.0f));
}

TEST_CASE(UiUnits, LayoutUnitFraction) {
    EXPECT(LayoutUnit::from_int_pixels(0).fraction() == 0);
    EXPECT(LayoutUnit::from_int_pixels(1).fraction() == 0);
    EXPECT((LayoutUnit::from_int_pixels(1) / 2).fraction() == LayoutUnit::precision() / 2);
    EXPECT((LayoutUnit::from_int_pixels(-1) / 2).fraction() == -LayoutUnit::precision() / 2);
}

TEST_CASE(UiUnits, LayoutUnitFloor) {
    EXPECT(LayoutUnit::from_int_pixels(0).floor() == 0);
    EXPECT(LayoutUnit::from_int_pixels(100000).floor() == 100000);
    EXPECT(LayoutUnit::from_int_pixels(-100000).floor() == -100000);

    EXPECT(LayoutUnit::from_float_pixels(0.1f).floor() == 0);
    EXPECT(LayoutUnit::from_float_pixels(0.5f).floor() == 0);
    EXPECT(LayoutUnit::from_float_pixels(0.9f).floor() == 0);
    EXPECT(LayoutUnit::from_float_pixels(1.1f).floor() == 1);
    EXPECT(LayoutUnit::from_float_pixels(-0.1f).floor() == -1);
    EXPECT(LayoutUnit::from_float_pixels(-0.5f).floor() == -1);
    EXPECT(LayoutUnit::from_float_pixels(-0.9f).floor() == -1);
    EXPECT(LayoutUnit::from_float_pixels(-1.1f).floor() == -2);
}

TEST_CASE(UiUnits, LayoutUnitRound) {
    EXPECT(LayoutUnit::from_int_pixels(0).round() == 0);
    EXPECT(LayoutUnit::from_int_pixels(100000).round() == 100000);
    EXPECT(LayoutUnit::from_int_pixels(-100000).round() == -100000);

    EXPECT(LayoutUnit::from_float_pixels(0.1f).round() == 0);
    EXPECT(LayoutUnit::from_float_pixels(0.49f).round() == 0);
    EXPECT(LayoutUnit::from_float_pixels(0.5f).round() == 1);
    EXPECT(LayoutUnit::from_float_pixels(0.51f).round() == 1);
    EXPECT(LayoutUnit::from_float_pixels(0.9f).round() == 1);
    EXPECT(LayoutUnit::from_float_pixels(1.1f).round() == 1);
    EXPECT(LayoutUnit::from_float_pixels(1.49f).round() == 1);
    EXPECT(LayoutUnit::from_float_pixels(1.5f).round() == 2);
    EXPECT(LayoutUnit::from_float_pixels(1.51f).round() == 2);

    EXPECT(LayoutUnit::from_float_pixels(-0.1f).round() == 0);
    EXPECT(LayoutUnit::from_float_pixels(-0.49f).round() == 0);
    EXPECT(LayoutUnit::from_float_pixels(-0.5f).round() == -1);
    EXPECT(LayoutUnit::from_float_pixels(-0.51f).round() == -1);
    EXPECT(LayoutUnit::from_float_pixels(-0.9f).round() == -1);
    EXPECT(LayoutUnit::from_float_pixels(-1.1f).round() == -1);
    EXPECT(LayoutUnit::from_float_pixels(-1.49f).round() == -1);
    EXPECT(LayoutUnit::from_float_pixels(-1.5f).round() == -2);
    EXPECT(LayoutUnit::from_float_pixels(-1.51f).round() == -2);
}

TEST_CASE(UiUnits, LayoutUnitCeil) {
    EXPECT(LayoutUnit::from_int_pixels(0).ceil() == 0);
    EXPECT(LayoutUnit::from_int_pixels(100000).ceil() == 100000);
    EXPECT(LayoutUnit::from_int_pixels(-100000).ceil() == -100000);

    EXPECT(LayoutUnit::from_float_pixels(0.1f).ceil() == 1);
    EXPECT(LayoutUnit::from_float_pixels(0.5f).ceil() == 1);
    EXPECT(LayoutUnit::from_float_pixels(0.9f).ceil() == 1);
    EXPECT(LayoutUnit::from_float_pixels(1.1f).ceil() == 2);
    EXPECT(LayoutUnit::from_float_pixels(-0.1f).ceil() == 0);
    EXPECT(LayoutUnit::from_float_pixels(-0.5f).ceil() == 0);
    EXPECT(LayoutUnit::from_float_pixels(-0.9f).ceil() == 0);
    EXPECT(LayoutUnit::from_float_pixels(-1.1f).ceil() == -1);
}

TEST_CASE(UiUnits, LayoutUnitToInt) {
    EXPECT(LayoutUnit::from_int_pixels(0).to_int() == 0);
    EXPECT(LayoutUnit::from_int_pixels(100000).to_int() == 100000);
    EXPECT(LayoutUnit::from_int_pixels(-100000).to_int() == -100000);

    EXPECT(LayoutUnit::from_float_pixels(0.1f).to_int() == 0);
    EXPECT(LayoutUnit::from_float_pixels(0.5f).to_int() == 0);
    EXPECT(LayoutUnit::from_float_pixels(0.9f).to_int() == 0);
    EXPECT(LayoutUnit::from_float_pixels(1.1f).to_int() == 1);
    EXPECT(LayoutUnit::from_float_pixels(-0.1f).to_int() == 0);
    EXPECT(LayoutUnit::from_float_pixels(-0.5f).to_int() == 0);
    EXPECT(LayoutUnit::from_float_pixels(-0.9f).to_int() == 0);
    EXPECT(LayoutUnit::from_float_pixels(-1.1f).to_int() == -1);
}
