#include <vull/container/vector.hh>

#include <vull/container/array.hh>
#include <vull/support/span.hh>
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
    Foo(const Foo &) = default;
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

// NOLINTBEGIN(readability-container-size-empty)

TEST_CASE(VectorTrivial, Empty) {
    Vector<int> vector;
    EXPECT_THAT(vector, is(empty()));
    EXPECT_THAT(vector.capacity(), is(equal_to(0)));
    EXPECT_THAT(vector.size(), is(equal_to(0)));
    EXPECT_THAT(vector.size_bytes(), is(equal_to(0)));
    EXPECT_THAT(vector.begin(), is(equal_to(vector.end())));
}

TEST_CASE(VectorTrivial, EnsureCapacity) {
    Vector<int> vector;
    vector.ensure_capacity(16);
    EXPECT_THAT(vector, is(empty()));
    EXPECT_THAT(vector.capacity(), is(equal_to(16)));
    EXPECT_THAT(vector.size(), is(equal_to(0)));
    EXPECT_THAT(vector.size_bytes(), is(equal_to(0)));
    EXPECT_THAT(vector.begin(), is(equal_to(vector.end())));
}

TEST_CASE(VectorTrivial, EnsureSize) {
    Vector<int> vector;
    vector.ensure_size(16);
    EXPECT_THAT(vector, is(not_(empty())));
    EXPECT_THAT(vector.capacity(), is(equal_to(16)));
    EXPECT_THAT(vector.size(), is(equal_to(16)));
    EXPECT_THAT(vector.size_bytes(), is(equal_to(16 * sizeof(int))));

    unsigned count = 0;
    for (int i : vector) {
        EXPECT_THAT(i, is(equal_to(0)));
        count++;
    }
    EXPECT_THAT(count, is(equal_to(vector.size())));
}

TEST_CASE(VectorTrivial, PushEmplace) {
    Vector<int> vector;
    vector.push(5);
    vector.emplace(10);
    EXPECT_THAT(vector, is(not_(empty())));
    EXPECT_TRUE(vector.capacity() >= 2); // TODO
    ASSERT_THAT(vector.size(), is(equal_to(2)));
    EXPECT_THAT(vector.size_bytes(), is(equal_to(2 * sizeof(int))));
    EXPECT_THAT(vector[0], is(equal_to(5)));
    EXPECT_THAT(vector[1], is(equal_to(10)));
}

TEST_CASE(VectorTrivial, Extend) {
    Vector<int> vector;
    vector.push(5);
    vector.push(10);
    vector.push(15);

    Vector<int> extended;
    extended.extend(vector);
    EXPECT_THAT(extended, is(not_(empty())));
    EXPECT_TRUE(extended.capacity() >= 3); // TODO
    ASSERT_THAT(extended.size(), is(equal_to(3)));
    EXPECT_THAT(extended[0], is(equal_to(5)));
    EXPECT_THAT(extended[1], is(equal_to(10)));
    EXPECT_THAT(extended[2], is(equal_to(15)));

    extended.extend(vector);
    EXPECT_THAT(extended, is(not_(empty())));
    EXPECT_TRUE(extended.capacity() >= 6); // TODO
    ASSERT_THAT(extended.size(), is(equal_to(6)));
    EXPECT_THAT(extended[0], is(equal_to(5)));
    EXPECT_THAT(extended[1], is(equal_to(10)));
    EXPECT_THAT(extended[2], is(equal_to(15)));
    EXPECT_THAT(extended[3], is(equal_to(5)));
    EXPECT_THAT(extended[4], is(equal_to(10)));
    EXPECT_THAT(extended[5], is(equal_to(15)));
}

TEST_CASE(VectorTrivial, Pop) {
    Vector<int> vector;
    vector.extend(Array{5, 10, 15});
    vector.pop();
    EXPECT_THAT(vector, is(not_(empty())));
    EXPECT_THAT(vector.size(), is(equal_to(2)));
    EXPECT_THAT(vector.size_bytes(), is(equal_to(2 * sizeof(int))));
    vector.pop();
    vector.pop();
    EXPECT_THAT(vector, is(empty()));
    EXPECT_THAT(vector.size(), is(equal_to(0)));
    EXPECT_THAT(vector.size_bytes(), is(equal_to(0)));
}

TEST_CASE(VectorTrivial, TakeLast) {
    Vector<int> vector;
    vector.extend(Array{5, 10, 15});
    EXPECT_THAT(vector.take_last(), is(equal_to(15)));
    EXPECT_THAT(vector, is(not_(empty())));
    EXPECT_THAT(vector.size(), is(equal_to(2)));
    EXPECT_THAT(vector.size_bytes(), is(equal_to(2 * sizeof(int))));
    EXPECT_THAT(vector.take_last(), is(equal_to(10)));
    EXPECT_THAT(vector.take_last(), is(equal_to(5)));
    EXPECT_THAT(vector, is(empty()));
    EXPECT_THAT(vector.size(), is(equal_to(0)));
    EXPECT_THAT(vector.size_bytes(), is(equal_to(0)));
}

TEST_CASE(VectorTrivial, Clear) {
    Vector<int> vector;
    vector.extend(Array{5, 10, 15});
    vector.clear();
    EXPECT_THAT(vector, is(empty()));
    EXPECT_THAT(vector.capacity(), is(equal_to(0)));
    EXPECT_THAT(vector.size(), is(equal_to(0)));
    EXPECT_THAT(vector.size_bytes(), is(equal_to(0)));
    EXPECT_THAT(vector.begin(), is(equal_to(vector.end())));

    // Reuse vector.
    vector.push(20);
    EXPECT_THAT(vector, is(not_(empty())));
    EXPECT_TRUE(vector.capacity() >= 1); // TODO
    ASSERT_THAT(vector.size(), is(equal_to(1)));
    EXPECT_THAT(vector.size_bytes(), is(equal_to(sizeof(int))));
    EXPECT_THAT(vector[0], is(equal_to(20)));
}

TEST_CASE(VectorTrivial, TakeAll) {
    Vector<int> vector;
    vector.extend(Array{5, 10, 15});

    auto span = vector.take_all();
    EXPECT_THAT(vector, is(empty()));
    EXPECT_THAT(vector.capacity(), is(equal_to(0)));
    EXPECT_THAT(vector.size(), is(equal_to(0)));
    EXPECT_THAT(vector.size_bytes(), is(equal_to(0)));
    EXPECT_THAT(vector.begin(), is(equal_to(vector.end())));
    EXPECT_THAT(span, is(not_(empty())));
    ASSERT_THAT(span.size(), is(equal_to(3)));
    EXPECT_THAT(span[0], is(equal_to(5)));
    EXPECT_THAT(span[1], is(equal_to(10)));
    EXPECT_THAT(span[2], is(equal_to(15)));
    delete[] span.data();
}

TEST_CASE(VectorTrivial, FirstLast) {
    Vector<int> vector;
    vector.extend(Array{5, 10, 15});
    EXPECT_THAT(vector.first(), is(equal_to(5)));
    EXPECT_THAT(vector.last(), is(equal_to(15)));
    vector.pop();
    EXPECT_THAT(vector.first(), is(equal_to(5)));
    EXPECT_THAT(vector.last(), is(equal_to(10)));
}

TEST_CASE(VectorTrivial, MoveConstruct) {
    Vector<int> vector;
    vector.extend(Array{5, 10, 15});

    Vector<int> moved(vull::move(vector));
    EXPECT_THAT(vector, is(empty()));
    EXPECT_THAT(vector.size(), is(equal_to(0)));
    EXPECT_THAT(vector.size_bytes(), is(equal_to(0)));
    EXPECT_THAT(vector.begin(), is(equal_to(vector.end())));
    EXPECT_THAT(moved, is(not_(empty())));
    ASSERT_THAT(moved.size(), is(equal_to(3)));
    EXPECT_THAT(moved[0], is(equal_to(5)));
    EXPECT_THAT(moved[1], is(equal_to(10)));
    EXPECT_THAT(moved[2], is(equal_to(15)));
}

TEST_CASE(VectorTrivial, MoveAssign) {
    Vector<int> vector;
    vector.extend(Array{5, 10, 15});

    Vector<int> moved;
    moved = vull::move(vector);
    EXPECT_THAT(vector, is(empty()));
    EXPECT_THAT(vector.size(), is(equal_to(0)));
    EXPECT_THAT(vector.size_bytes(), is(equal_to(0)));
    EXPECT_THAT(vector.begin(), is(equal_to(vector.end())));
    EXPECT_THAT(moved, is(not_(empty())));
    ASSERT_THAT(moved.size(), is(equal_to(3)));
    EXPECT_THAT(moved[0], is(equal_to(5)));
    EXPECT_THAT(moved[1], is(equal_to(10)));
    EXPECT_THAT(moved[2], is(equal_to(15)));
}

TEST_CASE(VectorTrivial, MoveAssignSelf) {
    Vector<int> vector;
    vector.extend(Array{5, 10, 15});
    vector = vull::move(vector);
    EXPECT_THAT(vector, is(not_(empty())));
    ASSERT_THAT(vector.size(), is(equal_to(3)));
    EXPECT_THAT(vector[0], is(equal_to(5)));
    EXPECT_THAT(vector[1], is(equal_to(10)));
    EXPECT_THAT(vector[2], is(equal_to(15)));
}

TEST_CASE(VectorObject, Empty) {
    Vector<Foo> vector;
    EXPECT_THAT(vector, is(empty()));
    EXPECT_THAT(vector.capacity(), is(equal_to(0)));
    EXPECT_THAT(vector.size(), is(equal_to(0)));
    EXPECT_THAT(vector.size_bytes(), is(equal_to(0)));
    EXPECT_THAT(vector.begin(), is(equal_to(vector.end())));
}

TEST_CASE(VectorObject, EnsureCapacity) {
    Vector<Foo> vector;
    vector.ensure_capacity(16);
    EXPECT_THAT(vector, is(empty()));
    EXPECT_THAT(vector.capacity(), is(equal_to(16)));
    EXPECT_THAT(vector.size(), is(equal_to(0)));
    EXPECT_THAT(vector.size_bytes(), is(equal_to(0)));
    EXPECT_THAT(vector.begin(), is(equal_to(vector.end())));
}

TEST_CASE(VectorObject, EnsureSize) {
    int destruct_count = 0;
    {
        Vector<Foo> vector;
        vector.ensure_size(16, destruct_count);
        EXPECT_THAT(vector, is(not_(empty())));
        EXPECT_THAT(vector.capacity(), is(equal_to(16)));
        EXPECT_THAT(vector.size(), is(equal_to(16)));
        EXPECT_THAT(vector.size_bytes(), is(equal_to(16 * sizeof(Foo))));
        EXPECT_THAT(destruct_count, is(equal_to(0)));

        unsigned count = 0;
        for ([[maybe_unused]] const auto &foo : vector) {
            count++;
        }
        EXPECT_THAT(count, is(equal_to(vector.size())));
        EXPECT_THAT(destruct_count, is(equal_to(0)));

        count = 0;
        for (auto foo : vector) {
            count++;
        }
        EXPECT_THAT(count, is(equal_to(vector.size())));
        EXPECT_THAT(destruct_count, is(equal_to(16)));
    }
    EXPECT_THAT(destruct_count, is(equal_to(32)));
}

TEST_CASE(VectorObject, Emplace) {
    int destruct_count = 0;
    {
        Vector<Foo> vector;
        vector.emplace(destruct_count);
        vector.emplace(destruct_count);
        EXPECT_THAT(vector, is(not_(empty())));
        EXPECT_TRUE(vector.capacity() >= 2); // TODO
        EXPECT_THAT(vector.size(), is(equal_to(2)));
        EXPECT_THAT(vector.size_bytes(), is(equal_to(2 * sizeof(Foo))));
        EXPECT_THAT(destruct_count, is(equal_to(0)));
    }
    EXPECT_THAT(destruct_count, is(equal_to(2)));
}

TEST_CASE(VectorObject, Push) {
    int destruct_count = 0;
    {
        Foo foo(destruct_count);
        Vector<Foo> vector;
        vector.push(foo);
        vector.push(vull::move(foo));
        EXPECT_THAT(vector, is(not_(empty())));
        EXPECT_TRUE(vector.capacity() >= 2); // TODO
        EXPECT_THAT(vector.size(), is(equal_to(2)));
        EXPECT_THAT(vector.size_bytes(), is(equal_to(2 * sizeof(Foo))));
        EXPECT_THAT(destruct_count, is(equal_to(0)));
    }
    EXPECT_THAT(destruct_count, is(equal_to(2)));
}

TEST_CASE(VectorObject, Extend) {
    int destruct_count = 0;
    {
        Vector<Foo> vector;
        vector.ensure_size(3, destruct_count);

        Vector<Foo> extended;
        extended.extend(vector);
        EXPECT_THAT(extended, is(not_(empty())));
        EXPECT_TRUE(extended.capacity() >= 3); // TODO
        EXPECT_THAT(extended.size(), is(equal_to(3)));

        extended.extend(vector);
        EXPECT_THAT(extended, is(not_(empty())));
        EXPECT_TRUE(extended.capacity() >= 6); // TODO
        EXPECT_THAT(extended.size(), is(equal_to(6)));
    }
    EXPECT_THAT(destruct_count, is(equal_to(9)));
}

TEST_CASE(VectorObject, PopTakeLast) {
    int destruct_count = 0;
    {
        Vector<Foo> vector;
        vector.ensure_size(3, destruct_count);
        vector.emplace(destruct_count);
        vector.pop();
        EXPECT_THAT(vector, is(not_(empty())));
        EXPECT_THAT(vector.size(), is(equal_to(3)));
        EXPECT_THAT(vector.size_bytes(), is(equal_to(3 * sizeof(Foo))));
        EXPECT_THAT(destruct_count, is(equal_to(1)));
        vector.take_last();
        vector.pop();
        vector.pop();
        EXPECT_THAT(vector, is(empty()));
        EXPECT_THAT(vector.size(), is(equal_to(0)));
        EXPECT_THAT(vector.size_bytes(), is(equal_to(0)));
        EXPECT_THAT(destruct_count, is(equal_to(4)));
    }
    EXPECT_THAT(destruct_count, is(equal_to(4)));
}

TEST_CASE(VectorObject, Clear) {
    int destruct_count = 0;
    Vector<Foo> vector;
    vector.ensure_size(16, destruct_count);
    vector.clear();
    EXPECT_THAT(vector, is(empty()));
    EXPECT_THAT(vector.capacity(), is(equal_to(0)));
    EXPECT_THAT(vector.size(), is(equal_to(0)));
    EXPECT_THAT(vector.size_bytes(), is(equal_to(0)));
    EXPECT_THAT(vector.begin(), is(equal_to(vector.end())));
    EXPECT_THAT(destruct_count, is(equal_to(16)));

    // Reuse vector.
    vector.emplace(destruct_count);
    EXPECT_THAT(vector, is(not_(empty())));
    EXPECT_TRUE(vector.capacity() >= 1); // TODO
    EXPECT_THAT(vector.size(), is(equal_to(1)));
    EXPECT_THAT(vector.size_bytes(), is(equal_to(sizeof(Foo))));
    vector.clear();
    EXPECT_THAT(destruct_count, is(equal_to(17)));
}

TEST_CASE(VectorObject, MoveConstruct) {
    int destruct_count = 0;
    Vector<Foo> vector;
    vector.ensure_size(16, destruct_count);

    Vector<Foo> moved(vull::move(vector));
    EXPECT_THAT(vector, is(empty()));
    EXPECT_THAT(vector.size(), is(equal_to(0)));
    EXPECT_THAT(vector.begin(), is(equal_to(vector.end())));
    EXPECT_THAT(moved, is(not_(empty())));
    EXPECT_THAT(moved.size(), is(equal_to(16)));

    EXPECT_THAT(destruct_count, is(equal_to(0)));
    moved.clear();
    EXPECT_THAT(destruct_count, is(equal_to(16)));
    vector.clear();
    EXPECT_THAT(destruct_count, is(equal_to(16)));
}

TEST_CASE(VectorObject, MoveAssign) {
    int destruct_count = 0;
    Vector<Foo> vector;
    vector.ensure_size(16, destruct_count);

    Vector<Foo> moved;
    moved = vull::move(vector);
    EXPECT_THAT(vector, is(empty()));
    EXPECT_THAT(vector.size(), is(equal_to(0)));
    EXPECT_THAT(vector.begin(), is(equal_to(vector.end())));
    EXPECT_THAT(moved, is(not_(empty())));
    EXPECT_THAT(moved.size(), is(equal_to(16)));

    EXPECT_THAT(destruct_count, is(equal_to(0)));
    moved.clear();
    EXPECT_THAT(destruct_count, is(equal_to(16)));
    vector.clear();
    EXPECT_THAT(destruct_count, is(equal_to(16)));
}

TEST_CASE(VectorObject, MoveAssignSelf) {
    int destruct_count = 0;
    Vector<Foo> vector;
    vector.ensure_size(16, destruct_count);
    vector = vull::move(vector);
    EXPECT_THAT(vector, is(not_(empty())));
    EXPECT_THAT(vector.size(), is(equal_to(16)));
    EXPECT_THAT(destruct_count, is(equal_to(0)));
    vector.clear();
    EXPECT_THAT(destruct_count, is(equal_to(16)));
}

// NOLINTEND(readability-container-size-empty)
