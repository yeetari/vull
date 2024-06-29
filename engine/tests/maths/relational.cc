#include <vull/maths/relational.hh>

#include <vull/maths/vec.hh>
#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/test.hh>

using namespace vull;
using namespace vull::test::matchers;

TEST_CASE(Relational, All) {
    EXPECT_TRUE(vull::all(Vec<bool, 1>(true)));
    EXPECT_TRUE(vull::all(Vec<bool, 2>(true)));
    EXPECT_FALSE(vull::all(Vec<bool, 1>(false)));
    EXPECT_FALSE(vull::all(Vec<bool, 2>(false)));
    EXPECT_FALSE(vull::all(Vec<bool, 2>(false, true)));
    EXPECT_FALSE(vull::all(Vec<bool, 2>(true, false)));
}

TEST_CASE(Relational, Any) {
    EXPECT_TRUE(vull::any(Vec<bool, 1>(true)));
    EXPECT_TRUE(vull::any(Vec<bool, 2>(true)));
    EXPECT_FALSE(vull::any(Vec<bool, 1>(false)));
    EXPECT_FALSE(vull::any(Vec<bool, 2>(false)));
    EXPECT_TRUE(vull::any(Vec<bool, 2>(false, true)));
    EXPECT_TRUE(vull::any(Vec<bool, 2>(true, false)));
}

TEST_CASE(Relational, Select) {
    auto vec = vull::select(Vec4u(1, 2, 3, 4), Vec4u(5, 6, 7, 8), Vec<bool, 4>(false, true, false, true));
    EXPECT_THAT(vec.x(), is(equal_to(1)));
    EXPECT_THAT(vec.y(), is(equal_to(6)));
    EXPECT_THAT(vec.z(), is(equal_to(3)));
    EXPECT_THAT(vec.w(), is(equal_to(8)));
}

TEST_CASE(Relational, Equal_Neither) {
    auto vec = vull::equal(Vec2u(5, 10), Vec2u(15, 20));
    EXPECT_FALSE(vec.x());
    EXPECT_FALSE(vec.y());
    EXPECT_FALSE(vull::all(vec));
    EXPECT_FALSE(vull::any(vec));
    EXPECT_TRUE(vull::all(vull::equal(vec, Vec<bool, 2>(false, false))));
}

TEST_CASE(Relational, Equal_One) {
    auto vec = vull::equal(Vec2u(5, 10), Vec2u(5, 15));
    EXPECT_TRUE(vec.x());
    EXPECT_FALSE(vec.y());
    EXPECT_FALSE(vull::all(vec));
    EXPECT_TRUE(vull::any(vec));
    EXPECT_TRUE(vull::all(vull::equal(vec, Vec<bool, 2>(true, false))));
}

TEST_CASE(Relational, Equal_Both) {
    auto vec = vull::equal(Vec2u(5, 5), Vec2u(5, 5));
    EXPECT_TRUE(vec.x());
    EXPECT_TRUE(vec.y());
    EXPECT_TRUE(vull::all(vec));
    EXPECT_TRUE(vull::any(vec));
    EXPECT_TRUE(vull::all(vull::equal(vec, Vec<bool, 2>(true, true))));
}

TEST_CASE(Relational, NotEqual_Neither) {
    auto vec = vull::not_equal(Vec2u(5, 5), Vec2u(5, 5));
    EXPECT_FALSE(vec.x());
    EXPECT_FALSE(vec.y());
    EXPECT_FALSE(vull::all(vec));
    EXPECT_FALSE(vull::any(vec));
    EXPECT_TRUE(vull::all(vull::not_equal(vec, Vec<bool, 2>(true, true))));
}

TEST_CASE(Relational, NotEqual_One) {
    auto vec = vull::not_equal(Vec2u(5, 10), Vec2u(5, 15));
    EXPECT_FALSE(vec.x());
    EXPECT_TRUE(vec.y());
    EXPECT_FALSE(vull::all(vec));
    EXPECT_TRUE(vull::any(vec));
    EXPECT_TRUE(vull::all(vull::not_equal(vec, Vec<bool, 2>(true, false))));
}

TEST_CASE(Relational, NotEqual_Both) {
    auto vec = vull::not_equal(Vec2u(5, 10), Vec2u(15, 20));
    EXPECT_TRUE(vec.x());
    EXPECT_TRUE(vec.y());
    EXPECT_TRUE(vull::all(vec));
    EXPECT_TRUE(vull::any(vec));
    EXPECT_TRUE(vull::all(vull::not_equal(vec, Vec<bool, 2>(false, false))));
}

TEST_CASE(Relational, LessThan) {
    auto vec = vull::less_than(Vec3u(5, 10, 15), Vec3u(4, 10, 16));
    EXPECT_FALSE(vec.x());
    EXPECT_FALSE(vec.y());
    EXPECT_TRUE(vec.z());
}

TEST_CASE(Relational, GreaterThan) {
    auto vec = vull::greater_than(Vec3u(5, 10, 15), Vec3u(4, 10, 16));
    EXPECT_TRUE(vec.x());
    EXPECT_FALSE(vec.y());
    EXPECT_FALSE(vec.z());
}

TEST_CASE(Relational, LessThanEqual) {
    auto vec = vull::less_than_equal(Vec3u(5, 10, 15), Vec3u(4, 10, 16));
    EXPECT_FALSE(vec.x());
    EXPECT_TRUE(vec.y());
    EXPECT_TRUE(vec.z());
}

TEST_CASE(Relational, GreaterThanEqual) {
    auto vec = vull::greater_than_equal(Vec3u(5, 10, 15), Vec3u(4, 10, 16));
    EXPECT_TRUE(vec.x());
    EXPECT_TRUE(vec.y());
    EXPECT_FALSE(vec.z());
}
