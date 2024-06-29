#include <vull/maths/epsilon.hh>

#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/test.hh>

#include <float.h>

using namespace vull;
using namespace vull::test::matchers;

TEST_CASE(Epsilon, Big) {
    EXPECT_THAT(1000000.0f, is(close_to(1000001.0f)));
    EXPECT_THAT(1000001.0f, is(close_to(1000000.0f)));
    EXPECT_THAT(10000.0f, is(not_(close_to(10001.0f))));
    EXPECT_THAT(10001.0f, is(not_(close_to(10000.0f))));
}

TEST_CASE(Epsilon, BigNegative) {
    EXPECT_THAT(-1000000.0f, is(close_to(-1000001.0f)));
    EXPECT_THAT(-1000001.0f, is(close_to(-1000000.0f)));
    EXPECT_THAT(-10000.0f, is(not_(close_to(-10001.0f))));
    EXPECT_THAT(-10001.0f, is(not_(close_to(-10000.0f))));
}

TEST_CASE(Epsilon, Small) {
    EXPECT_THAT(0.0001f, is(not_(close_to(0.0002f))));
    EXPECT_THAT(0.0002f, is(not_(close_to(0.0001f))));
    EXPECT_THAT(0.000000001000001f, is(close_to(0.000000001000002f)));
    EXPECT_THAT(0.000000001000002f, is(close_to(0.000000001000001f)));
    EXPECT_THAT(0.000000000001002f, is(close_to(0.000000000001001f)));
    EXPECT_THAT(0.000000000001001f, is(close_to(0.000000000001002f)));
}

TEST_CASE(Epsilon, SmallNegative) {
    EXPECT_THAT(-0.0001f, is(not_(close_to(-0.0002f))));
    EXPECT_THAT(-0.0002f, is(not_(close_to(-0.0001f))));
    EXPECT_THAT(-0.000000001000001f, is(close_to(-0.000000001000002f)));
    EXPECT_THAT(-0.000000001000002f, is(close_to(-0.000000001000001f)));
    EXPECT_THAT(-0.000000000001002f, is(close_to(-0.000000000001001f)));
    EXPECT_THAT(-0.000000000001001f, is(close_to(-0.000000000001002f)));
}

TEST_CASE(Epsilon, NearOne) {
    EXPECT_THAT(1.0000001f, is(close_to(1.0000002f)));
    EXPECT_THAT(1.0000002f, is(close_to(1.0000001f)));
    EXPECT_THAT(1.0002f, is(not_(close_to(1.0001f))));
    EXPECT_THAT(1.0001f, is(not_(close_to(1.0002f))));
}

TEST_CASE(Epsilon, NearOneNegative) {
    EXPECT_THAT(-1.000001f, is(close_to(-1.000002f)));
    EXPECT_THAT(-1.000002f, is(close_to(-1.000001f)));
    EXPECT_THAT(-1.0001f, is(not_(close_to(-1.0002f))));
    EXPECT_THAT(-1.0002f, is(not_(close_to(-1.0001f))));
}

TEST_CASE(Epsilon, SmallDifferences) {
    EXPECT_THAT(0.3f, is(close_to(0.30000003f)));
    EXPECT_THAT(-0.3f, is(close_to(-0.30000003f)));
}

TEST_CASE(Epsilon, NearMax) {
    EXPECT_THAT(FLT_MAX, is(close_to(FLT_MAX)));
    EXPECT_THAT(FLT_MAX, is(not_(close_to(-FLT_MAX))));
    EXPECT_THAT(-FLT_MAX, is(not_(close_to(FLT_MAX))));
    EXPECT_THAT(FLT_MAX, is(not_(close_to(FLT_MAX / 2))));
    EXPECT_THAT(FLT_MAX, is(not_(close_to(-FLT_MAX / 2))));
    EXPECT_THAT(-FLT_MAX, is(not_(close_to(FLT_MAX / 2))));

    EXPECT_THAT(FLT_MAX, is(not_(close_to_zero())));
    EXPECT_THAT(-FLT_MAX, is(not_(close_to_zero())));
}

TEST_CASE(Epsilon, NearZero) {
    EXPECT_THAT(0.0f, is(close_to(0.0f)));
    EXPECT_THAT(0.0f, is(close_to(-0.0f)));
    EXPECT_THAT(-0.0f, is(close_to(-0.0f)));
    EXPECT_THAT(0.00000001f, is(close_to(0.0f)));
    EXPECT_THAT(-0.00000001f, is(close_to(0.0f)));
    EXPECT_THAT(FLT_MIN, is(close_to(0.0f)));
    EXPECT_THAT(FLT_EPSILON, is(close_to(0.0f)));

    EXPECT_THAT(0.0f, is(close_to_zero()));
    EXPECT_THAT(-0.0f, is(close_to_zero()));
    EXPECT_THAT(0.00000001f, is(close_to_zero()));
    EXPECT_THAT(-0.00000001f, is(close_to_zero()));
    EXPECT_THAT(FLT_MIN, is(close_to_zero()));
    EXPECT_THAT(FLT_EPSILON, is(close_to_zero()));
}
