#include <vull/container/vector.hh>

#include <vull/container/array.hh>
#include <vull/support/span.hh>
#include <vull/support/utility.hh>
#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/move_tester.hh>
#include <vull/test/test.hh>

#include <stddef.h>

using namespace vull;
using namespace vull::test::matchers;

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

TEST_CASE(VectorTrivial, PushInternalReference) {
    Vector<int> vector;
    vector.push(5);
    for (uint32_t i = 1; i < vector.capacity(); i++) {
        vector.push(vector.first());
    }
    vector.push(vector.last());
    for (int elem : vector) {
        EXPECT_THAT(elem, is(equal_to(5)));
    }
    EXPECT_THAT(vector.last(), is(equal_to(5)));
}

TEST_CASE(VectorTrivial, EmplaceInternalReference) {
    Vector<int> vector;
    vector.push(5);
    for (uint32_t i = 1; i < vector.capacity(); i++) {
        vector.emplace(vector.first());
    }
    vector.emplace(vector.last());
    for (int elem : vector) {
        EXPECT_THAT(elem, is(equal_to(5)));
    }
    EXPECT_THAT(vector.last(), is(equal_to(5)));
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
    Vector<test::MoveTester> vector;
    EXPECT_THAT(vector, is(empty()));
    EXPECT_THAT(vector.capacity(), is(equal_to(0)));
    EXPECT_THAT(vector.size(), is(equal_to(0)));
    EXPECT_THAT(vector.size_bytes(), is(equal_to(0)));
    EXPECT_THAT(vector.begin(), is(equal_to(vector.end())));
}

TEST_CASE(VectorObject, EnsureCapacity) {
    Vector<test::MoveTester> vector;
    vector.ensure_capacity(16);
    EXPECT_THAT(vector, is(empty()));
    EXPECT_THAT(vector.capacity(), is(equal_to(16)));
    EXPECT_THAT(vector.size(), is(equal_to(0)));
    EXPECT_THAT(vector.size_bytes(), is(equal_to(0)));
    EXPECT_THAT(vector.begin(), is(equal_to(vector.end())));
}

TEST_CASE(VectorObject, EnsureSize) {
    size_t destruct_count = 0;
    {
        Vector<test::MoveTester> vector;
        vector.ensure_size(16, destruct_count);
        EXPECT_THAT(vector, is(not_(empty())));
        EXPECT_THAT(vector.capacity(), is(equal_to(16)));
        EXPECT_THAT(vector.size(), is(equal_to(16)));
        EXPECT_THAT(vector.size_bytes(), is(equal_to(16 * sizeof(test::MoveTester))));
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
    size_t destruct_count = 0;
    {
        Vector<test::MoveTester> vector;
        vector.emplace(destruct_count);
        vector.emplace(destruct_count);
        EXPECT_THAT(vector, is(not_(empty())));
        EXPECT_TRUE(vector.capacity() >= 2); // TODO
        EXPECT_THAT(vector.size(), is(equal_to(2)));
        EXPECT_THAT(vector.size_bytes(), is(equal_to(2 * sizeof(test::MoveTester))));
        EXPECT_THAT(destruct_count, is(equal_to(0)));
    }
    EXPECT_THAT(destruct_count, is(equal_to(2)));
}

TEST_CASE(VectorObject, Push) {
    size_t destruct_count = 0;
    {
        test::MoveTester foo(destruct_count);
        Vector<test::MoveTester> vector;
        vector.push(foo);
        vector.push(vull::move(foo));
        EXPECT_THAT(vector, is(not_(empty())));
        EXPECT_TRUE(vector.capacity() >= 2); // TODO
        EXPECT_THAT(vector.size(), is(equal_to(2)));
        EXPECT_THAT(vector.size_bytes(), is(equal_to(2 * sizeof(test::MoveTester))));
        EXPECT_THAT(destruct_count, is(equal_to(0)));
    }
    EXPECT_THAT(destruct_count, is(equal_to(2)));
}

TEST_CASE(VectorObject, EmplaceInternalReference) {
    size_t destruct_count = 0;
    uint32_t expected_size = 2;
    {
        Vector<test::MoveTester> vector;
        vector.emplace(destruct_count);
        for (uint32_t i = 1; i < vector.capacity(); i++) {
            vector.emplace(destruct_count);
            expected_size++;
        }
        vector.emplace(vector.last());
        EXPECT_THAT(vector.size(), is(equal_to(expected_size)));
        EXPECT_THAT(destruct_count, is(equal_to(0)));
    }
    EXPECT_THAT(destruct_count, is(equal_to(expected_size)));
}

TEST_CASE(VectorObject, PushInternalReference) {
    size_t destruct_count = 0;
    uint32_t expected_size = 2;
    {
        Vector<test::MoveTester> vector;
        vector.emplace(destruct_count);
        for (uint32_t i = 1; i < vector.capacity(); i++) {
            vector.emplace(destruct_count);
            expected_size++;
        }
        vector.push(vector.last());
        EXPECT_THAT(vector.size(), is(equal_to(expected_size)));
        EXPECT_THAT(destruct_count, is(equal_to(0)));
    }
    EXPECT_THAT(destruct_count, is(equal_to(expected_size)));
}

TEST_CASE(VectorObject, PushMoveInternalReference) {
    size_t destruct_count = 0;
    uint32_t expected_size = 2;
    {
        Vector<test::MoveTester> vector;
        vector.emplace(destruct_count);
        for (uint32_t i = 1; i < vector.capacity(); i++) {
            vector.emplace(destruct_count);
            expected_size++;
        }
        vector.push(vull::move(vector.last()));
        EXPECT_THAT(vector.size(), is(equal_to(expected_size)));
        EXPECT_THAT(destruct_count, is(equal_to(0)));
    }
    EXPECT_THAT(destruct_count, is(equal_to(expected_size - 1)));
}

TEST_CASE(VectorObject, Extend) {
    size_t destruct_count = 0;
    {
        Vector<test::MoveTester> vector;
        vector.ensure_size(3, destruct_count);

        Vector<test::MoveTester> extended;
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
    size_t destruct_count = 0;
    {
        Vector<test::MoveTester> vector;
        vector.ensure_size(3, destruct_count);
        vector.emplace(destruct_count);
        vector.pop();
        EXPECT_THAT(vector, is(not_(empty())));
        EXPECT_THAT(vector.size(), is(equal_to(3)));
        EXPECT_THAT(vector.size_bytes(), is(equal_to(3 * sizeof(test::MoveTester))));
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
    size_t destruct_count = 0;
    Vector<test::MoveTester> vector;
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
    EXPECT_THAT(vector.size_bytes(), is(equal_to(sizeof(test::MoveTester))));
    vector.clear();
    EXPECT_THAT(destruct_count, is(equal_to(17)));
}

TEST_CASE(VectorObject, MoveConstruct) {
    size_t destruct_count = 0;
    Vector<test::MoveTester> vector;
    vector.ensure_size(16, destruct_count);

    Vector<test::MoveTester> moved(vull::move(vector));
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
    size_t destruct_count = 0;
    Vector<test::MoveTester> vector;
    vector.ensure_size(16, destruct_count);

    Vector<test::MoveTester> moved;
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
    size_t destruct_count = 0;
    Vector<test::MoveTester> vector;
    vector.ensure_size(16, destruct_count);
    vector = vull::move(vector);
    EXPECT_THAT(vector, is(not_(empty())));
    EXPECT_THAT(vector.size(), is(equal_to(16)));
    EXPECT_THAT(destruct_count, is(equal_to(0)));
    vector.clear();
    EXPECT_THAT(destruct_count, is(equal_to(16)));
}

// NOLINTEND(readability-container-size-empty)
