#include <vull/support/variant.hh>

#include <vull/support/utility.hh>
#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/test.hh>

using namespace vull;
using namespace vull::test::matchers;

namespace {

class Foo {
    int *m_destruct_count{nullptr};

public:
    explicit Foo(int &destruct_count) : m_destruct_count(&destruct_count) {}
    Foo(const Foo &) = delete;
    Foo(Foo &&other) : m_destruct_count(vull::exchange(other.m_destruct_count, nullptr)) {}
    ~Foo() {
        if (m_destruct_count != nullptr) {
            (*m_destruct_count)++;
        }
    }

    Foo &operator=(const Foo &) = delete;
    Foo &operator=(Foo &&) = delete;
};

} // namespace

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
    int destruct_count = 0;
    {
        Variant<int, Foo> variant(Foo{destruct_count});
        EXPECT_THAT(variant, is(of_type<Foo>()));
        EXPECT_THAT(destruct_count, is(equal_to(0)));

        variant = Variant<int, Foo>(Foo{destruct_count});
        EXPECT_THAT(variant, is(of_type<Foo>()));
        EXPECT_THAT(destruct_count, is(equal_to(1)));

        variant.set(5);
        EXPECT_THAT(variant, is(equal_to(5)));
        EXPECT_THAT(destruct_count, is(equal_to(2)));

        variant.set(Foo{destruct_count});
        EXPECT_THAT(variant, is(of_type<Foo>()));
        EXPECT_THAT(destruct_count, is(equal_to(2)));
    }
    EXPECT_THAT(destruct_count, is(equal_to(3)));
}
