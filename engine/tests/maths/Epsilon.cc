#include <vull/maths/Epsilon.hh>

#include <vull/support/Assert.hh>
#include <vull/support/Test.hh>

#include <float.h>

using namespace vull;

TEST_SUITE(Epsilon, {
    ;
    TEST_CASE(Big) {
        EXPECT(vull::fuzzy_equal(1000000.0f, 1000001.0f));
        EXPECT(vull::fuzzy_equal(1000001.0f, 1000000.0f));
        EXPECT(!vull::fuzzy_equal(10000.0f, 10001.0f));
        EXPECT(!vull::fuzzy_equal(10001.0f, 10000.0f));
    }

    TEST_CASE(BigNegative) {
        EXPECT(vull::fuzzy_equal(-1000000.0f, -1000001.0f));
        EXPECT(vull::fuzzy_equal(-1000001.0f, -1000000.0f));
        EXPECT(!vull::fuzzy_equal(-10000.0f, -10001.0f));
        EXPECT(!vull::fuzzy_equal(-10001.0f, -10000.0f));
    }

    TEST_CASE(Small) {
        EXPECT(!vull::fuzzy_equal(0.0001f, 0.0002f));
        EXPECT(!vull::fuzzy_equal(0.0002f, 0.0001f));
        EXPECT(vull::fuzzy_equal(0.000000001000001f, 0.000000001000002f));
        EXPECT(vull::fuzzy_equal(0.000000001000002f, 0.000000001000001f));
        EXPECT(vull::fuzzy_equal(0.000000000001002f, 0.000000000001001f));
        EXPECT(vull::fuzzy_equal(0.000000000001001f, 0.000000000001002f));
    }

    TEST_CASE(SmallNegative) {
        EXPECT(!vull::fuzzy_equal(-0.0001f, -0.0002f));
        EXPECT(!vull::fuzzy_equal(-0.0002f, -0.0001f));
        EXPECT(vull::fuzzy_equal(-0.000000001000001f, -0.000000001000002f));
        EXPECT(vull::fuzzy_equal(-0.000000001000002f, -0.000000001000001f));
        EXPECT(vull::fuzzy_equal(-0.000000000001002f, -0.000000000001001f));
        EXPECT(vull::fuzzy_equal(-0.000000000001001f, -0.000000000001002f));
    }

    TEST_CASE(NearOne) {
        EXPECT(vull::fuzzy_equal(1.0000001f, 1.0000002f));
        EXPECT(vull::fuzzy_equal(1.0000002f, 1.0000001f));
        EXPECT(!vull::fuzzy_equal(1.0002f, 1.0001f));
        EXPECT(!vull::fuzzy_equal(1.0001f, 1.0002f));
    }

    TEST_CASE(NearOneNegative) {
        EXPECT(vull::fuzzy_equal(-1.000001f, -1.000002f));
        EXPECT(vull::fuzzy_equal(-1.000002f, -1.000001f));
        EXPECT(!vull::fuzzy_equal(-1.0001f, -1.0002f));
        EXPECT(!vull::fuzzy_equal(-1.0002f, -1.0001f));
    }

    TEST_CASE(SmallDifferences) {
        EXPECT(vull::fuzzy_equal(0.3f, 0.30000003f));
        EXPECT(vull::fuzzy_equal(-0.3f, -0.30000003f));
    }

    TEST_CASE(NearMax) {
        EXPECT(vull::fuzzy_equal(FLT_MAX, FLT_MAX));
        EXPECT(!vull::fuzzy_equal(FLT_MAX, -FLT_MAX));
        EXPECT(!vull::fuzzy_equal(-FLT_MAX, FLT_MAX));
        EXPECT(!vull::fuzzy_equal(FLT_MAX, FLT_MAX / 2));
        EXPECT(!vull::fuzzy_equal(FLT_MAX, -FLT_MAX / 2));
        EXPECT(!vull::fuzzy_equal(-FLT_MAX, FLT_MAX / 2));

        EXPECT(!vull::fuzzy_zero(FLT_MAX));
        EXPECT(!vull::fuzzy_zero(-FLT_MAX));
    }

    TEST_CASE(NearZero) {
        EXPECT(vull::fuzzy_equal(0.0f, 0.0f));
        EXPECT(vull::fuzzy_equal(0.0f, -0.0f));
        EXPECT(vull::fuzzy_equal(-0.0f, -0.0f));
        EXPECT(vull::fuzzy_equal(0.00000001f, 0.0f));
        EXPECT(vull::fuzzy_equal(-0.00000001f, 0.0f));
        EXPECT(vull::fuzzy_equal(FLT_MIN, 0.0f));
        EXPECT(vull::fuzzy_equal(FLT_EPSILON, 0.0f));

        EXPECT(vull::fuzzy_zero(0.0f));
        EXPECT(vull::fuzzy_zero(-0.0f));
        EXPECT(vull::fuzzy_zero(0.00000001f));
        EXPECT(vull::fuzzy_zero(-0.00000001f));
        EXPECT(vull::fuzzy_zero(FLT_MIN));
        EXPECT(vull::fuzzy_zero(FLT_EPSILON));
    }
})
