#include <vull/support/variant.hh>

#include <vull/support/assert.hh>
#include <vull/support/test.hh>
#include <vull/support/utility.hh>

using namespace vull;

namespace {

class Foo {
    int *m_destruct_count{nullptr};

public:
    Foo() = default;
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
    EXPECT(!variant.has<float>());
    EXPECT(variant.has<int>());
    EXPECT(variant.get<int>() == 5);

    variant.set(10);
    EXPECT(!variant.has<float>());
    EXPECT(variant.has<int>());
    EXPECT(variant.get<int>() == 10);

    variant.set(1.0f);
    EXPECT(!variant.has<int>());
    EXPECT(variant.has<float>());
    EXPECT(variant.get<float>() == 1.0f);
}

TEST_CASE(Variant, TrivialDowncast) {
    Variant<int, float, double> variant(5.0f);
    EXPECT(variant.has<float>());

    auto downcasted = variant.downcast<float, double>();
    EXPECT(downcasted.has<float>());
    EXPECT(downcasted.get<float>() == 5.0f);

    variant.set(8.0);
    downcasted.set(10.0);
    EXPECT(variant.has<double>());
    EXPECT(downcasted.has<double>());
    EXPECT(variant.get<double>() == 8.0);
    EXPECT(downcasted.get<double>() == 10.0);
}

TEST_CASE(Variant, DestructMove) {
    int destruct_count = 0;
    {
        Variant<int, Foo> variant(Foo{destruct_count});
        EXPECT(variant.has<Foo>());
        EXPECT(destruct_count == 0);

        variant = Variant<int, Foo>(Foo{destruct_count});
        EXPECT(variant.has<Foo>());
        EXPECT(destruct_count == 1);

        variant.set(5);
        EXPECT(variant.has<int>());
        EXPECT(destruct_count == 2);

        variant.set(Foo{destruct_count});
        EXPECT(variant.has<Foo>());
        EXPECT(destruct_count == 2);
    }
    EXPECT(destruct_count == 3);
}
