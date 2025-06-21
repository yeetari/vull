#include <vull/support/variant.hh>

#include <vull/support/utility.hh>
#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/move_tester.hh>
#include <vull/test/test.hh>

using namespace vull;
using namespace vull::test::matchers;

TEST_CASE(Variant, Trivial) {
    Variant<int, float> variant(5);
    EXPECT_THAT(variant, is(equal_to(5)));

    variant.set(10);
    EXPECT_THAT(variant, is(equal_to(10)));

    variant.set(1.0f);
    EXPECT_THAT(variant, is(equal_to(1.0f)));
}

TEST_CASE(Variant, TrivialDowncast) {
    Variant<int, float, double> variant(5.0f);
    EXPECT_THAT(variant, is(equal_to(5.0f)));

    auto downcasted = variant.downcast<float, double>();
    EXPECT_THAT(downcasted, is(equal_to(5.0f)));

    variant.set(8.0);
    downcasted.set(10.0);
    EXPECT_THAT(variant, is(equal_to(8.0)));
    EXPECT_THAT(downcasted, is(equal_to(10.0)));
}

TEST_CASE(Variant, DestructMove) {
    size_t destruct_count = 0;
    {
        Variant<int, test::MoveTester> variant(test::MoveTester{destruct_count});
        EXPECT_THAT(variant, is(of_type<test::MoveTester>()));
        EXPECT_THAT(destruct_count, is(equal_to(0)));

        variant = Variant<int, test::MoveTester>(test::MoveTester{destruct_count});
        EXPECT_THAT(variant, is(of_type<test::MoveTester>()));
        EXPECT_THAT(destruct_count, is(equal_to(1)));

        variant.set(5);
        EXPECT_THAT(variant, is(equal_to(5)));
        EXPECT_THAT(destruct_count, is(equal_to(2)));

        variant.set(test::MoveTester{destruct_count});
        EXPECT_THAT(variant, is(of_type<test::MoveTester>()));
        EXPECT_THAT(destruct_count, is(equal_to(2)));
    }
    EXPECT_THAT(destruct_count, is(equal_to(3)));
}
