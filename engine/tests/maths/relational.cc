#include <vull/maths/relational.hh>

#include <vull/maths/vec.hh>
#include <vull/support/test.hh>

using namespace vull;

TEST_CASE(Relational, All) {
    EXPECT(vull::all(Vec<bool, 1>(true)));
    EXPECT(vull::all(Vec<bool, 2>(true)));
    EXPECT(!vull::all(Vec<bool, 1>(false)));
    EXPECT(!vull::all(Vec<bool, 2>(false)));
    EXPECT(!vull::all(Vec<bool, 2>(false, true)));
    EXPECT(!vull::all(Vec<bool, 2>(true, false)));
}

TEST_CASE(Relational, Any) {
    EXPECT(vull::any(Vec<bool, 1>(true)));
    EXPECT(vull::any(Vec<bool, 2>(true)));
    EXPECT(!vull::any(Vec<bool, 1>(false)));
    EXPECT(!vull::any(Vec<bool, 2>(false)));
    EXPECT(vull::any(Vec<bool, 2>(false, true)));
    EXPECT(vull::any(Vec<bool, 2>(true, false)));
}

TEST_CASE(Relational, Select) {
    auto vec = vull::select(Vec4u(1, 2, 3, 4), Vec4u(5, 6, 7, 8), Vec<bool, 4>(false, true, false, true));
    EXPECT(vec.x() == 1);
    EXPECT(vec.y() == 6);
    EXPECT(vec.z() == 3);
    EXPECT(vec.w() == 8);
}

TEST_CASE(Relational, Equal_Neither) {
    auto vec = vull::equal(Vec2u(5, 10), Vec2u(15, 20));
    EXPECT(!vec.x() && !vec.y());
    EXPECT(!vull::all(vec));
    EXPECT(!vull::any(vec));
    EXPECT(vull::all(vull::equal(vec, Vec<bool, 2>(false, false))));
}

TEST_CASE(Relational, Equal_One) {
    auto vec = vull::equal(Vec2u(5, 10), Vec2u(5, 15));
    EXPECT(vec.x() && !vec.y());
    EXPECT(!vull::all(vec));
    EXPECT(vull::any(vec));
    EXPECT(vull::all(vull::equal(vec, Vec<bool, 2>(true, false))));
}

TEST_CASE(Relational, Equal_Both) {
    auto vec = vull::equal(Vec2u(5, 5), Vec2u(5, 5));
    EXPECT(vec.x() && vec.y());
    EXPECT(vull::all(vec));
    EXPECT(vull::any(vec));
    EXPECT(vull::all(vull::equal(vec, Vec<bool, 2>(true, true))));
}

TEST_CASE(Relational, NotEqual_Neither) {
    auto vec = vull::not_equal(Vec2u(5, 5), Vec2u(5, 5));
    EXPECT(!vec.x() && !vec.y());
    EXPECT(!vull::all(vec));
    EXPECT(!vull::any(vec));
    EXPECT(vull::all(vull::not_equal(vec, Vec<bool, 2>(true, true))));
}

TEST_CASE(Relational, NotEqual_One) {
    auto vec = vull::not_equal(Vec2u(5, 10), Vec2u(5, 15));
    EXPECT(!vec.x() && vec.y());
    EXPECT(!vull::all(vec));
    EXPECT(vull::any(vec));
    EXPECT(vull::all(vull::not_equal(vec, Vec<bool, 2>(true, false))));
}

TEST_CASE(Relational, NotEqual_Both) {
    auto vec = vull::not_equal(Vec2u(5, 10), Vec2u(15, 20));
    EXPECT(vec.x() && vec.y());
    EXPECT(vull::all(vec));
    EXPECT(vull::any(vec));
    EXPECT(vull::all(vull::not_equal(vec, Vec<bool, 2>(false, false))));
}

TEST_CASE(Relational, LessThan) {
    auto vec = vull::less_than(Vec3u(5, 10, 15), Vec3u(4, 10, 16));
    EXPECT(!vec.x() && !vec.y() && vec.z());
}

TEST_CASE(Relational, GreaterThan) {
    auto vec = vull::greater_than(Vec3u(5, 10, 15), Vec3u(4, 10, 16));
    EXPECT(vec.x() && !vec.y() && !vec.z());
}

TEST_CASE(Relational, LessThanEqual) {
    auto vec = vull::less_than_equal(Vec3u(5, 10, 15), Vec3u(4, 10, 16));
    EXPECT(!vec.x() && vec.y() && vec.z());
}

TEST_CASE(Relational, GreaterThanEqual) {
    auto vec = vull::greater_than_equal(Vec3u(5, 10, 15), Vec3u(4, 10, 16));
    EXPECT(vec.x() && vec.y() && !vec.z());
}
