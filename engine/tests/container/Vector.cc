#include <vull/container/Vector.hh>

#include <vull/container/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Span.hh>
#include <vull/support/Test.hh>
#include <vull/support/Utility.hh>

using namespace vull;

namespace {

class Foo {
    int *m_destruct_count{nullptr};

public:
    Foo() = default;
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
    EXPECT(vector.empty());
    EXPECT(vector.capacity() == 0);
    EXPECT(vector.size() == 0);
    EXPECT(vector.size_bytes() == 0);
    EXPECT(vector.begin() == vector.end());
}

TEST_CASE(VectorTrivial, EnsureCapacity) {
    Vector<int> vector;
    vector.ensure_capacity(16);
    EXPECT(vector.empty());
    EXPECT(vector.capacity() == 16);
    EXPECT(vector.size() == 0);
    EXPECT(vector.size_bytes() == 0);
    EXPECT(vector.begin() == vector.end());
}

TEST_CASE(VectorTrivial, EnsureSize) {
    Vector<int> vector;
    vector.ensure_size(16);
    EXPECT(!vector.empty());
    EXPECT(vector.capacity() == 16);
    EXPECT(vector.size() == 16);
    EXPECT(vector.size_bytes() == 16 * sizeof(int));

    unsigned count = 0;
    for (int i : vector) {
        EXPECT(i == 0);
        count++;
    }
    EXPECT(count == vector.size());
}

TEST_CASE(VectorTrivial, PushEmplace) {
    Vector<int> vector;
    vector.push(5);
    vector.emplace(10);
    EXPECT(!vector.empty());
    EXPECT(vector.capacity() >= 2);
    EXPECT(vector.size() == 2);
    EXPECT(vector.size_bytes() == 2 * sizeof(int));
    EXPECT(vector[0] == 5);
    EXPECT(vector[1] == 10);
}

TEST_CASE(VectorTrivial, Extend) {
    Vector<int> vector;
    vector.push(5);
    vector.push(10);
    vector.push(15);

    Vector<int> extended;
    extended.extend(vector);
    EXPECT(!extended.empty());
    EXPECT(extended.capacity() >= 3);
    EXPECT(extended.size() == 3);
    EXPECT(extended[0] == 5);
    EXPECT(extended[1] == 10);
    EXPECT(extended[2] == 15);

    extended.extend(vector);
    EXPECT(!extended.empty());
    EXPECT(extended.capacity() >= 6);
    EXPECT(extended.size() == 6);
    EXPECT(extended[0] == 5);
    EXPECT(extended[1] == 10);
    EXPECT(extended[2] == 15);
    EXPECT(extended[3] == 5);
    EXPECT(extended[4] == 10);
    EXPECT(extended[5] == 15);
}

TEST_CASE(VectorTrivial, Pop) {
    Vector<int> vector;
    vector.extend(Array{5, 10, 15});
    vector.pop();
    EXPECT(!vector.empty());
    EXPECT(vector.size() == 2);
    EXPECT(vector.size_bytes() == 2 * sizeof(int));
    vector.pop();
    vector.pop();
    EXPECT(vector.empty());
    EXPECT(vector.size() == 0);
    EXPECT(vector.size_bytes() == 0);
}

TEST_CASE(VectorTrivial, TakeLast) {
    Vector<int> vector;
    vector.extend(Array{5, 10, 15});
    EXPECT(vector.take_last() == 15);
    EXPECT(!vector.empty());
    EXPECT(vector.size() == 2);
    EXPECT(vector.size_bytes() == 2 * sizeof(int));
    EXPECT(vector.take_last() == 10);
    EXPECT(vector.take_last() == 5);
    EXPECT(vector.empty());
    EXPECT(vector.size() == 0);
    EXPECT(vector.size_bytes() == 0);
}

TEST_CASE(VectorTrivial, Clear) {
    Vector<int> vector;
    vector.extend(Array{5, 10, 15});
    vector.clear();
    EXPECT(vector.empty());
    EXPECT(vector.capacity() == 0);
    EXPECT(vector.size() == 0);
    EXPECT(vector.size_bytes() == 0);
    EXPECT(vector.begin() == vector.end());

    // Reuse vector.
    vector.push(20);
    EXPECT(!vector.empty());
    EXPECT(vector.capacity() >= 1);
    EXPECT(vector.size() == 1);
    EXPECT(vector.size_bytes() == sizeof(int));
    EXPECT(vector[0] == 20);
}

TEST_CASE(VectorTrivial, TakeAll) {
    Vector<int> vector;
    vector.extend(Array{5, 10, 15});

    auto span = vector.take_all();
    EXPECT(vector.empty());
    EXPECT(vector.capacity() == 0);
    EXPECT(vector.size() == 0);
    EXPECT(vector.size_bytes() == 0);
    EXPECT(vector.begin() == vector.end());
    EXPECT(!span.empty());
    EXPECT(span.size() == 3);
    EXPECT(span[0] == 5);
    EXPECT(span[1] == 10);
    EXPECT(span[2] == 15);
    delete[] span.data();
}

TEST_CASE(VectorTrivial, FirstLast) {
    Vector<int> vector;
    vector.extend(Array{5, 10, 15});
    EXPECT(vector.first() == 5);
    EXPECT(vector.last() == 15);
    vector.pop();
    EXPECT(vector.first() == 5);
    EXPECT(vector.last() == 10);
}

TEST_CASE(VectorTrivial, MoveConstruct) {
    Vector<int> vector;
    vector.extend(Array{5, 10, 15});

    Vector<int> moved(vull::move(vector));
    EXPECT(vector.empty());
    EXPECT(vector.size() == 0);
    EXPECT(vector.begin() == vector.end());
    EXPECT(!moved.empty());
    EXPECT(moved.size() == 3);
    EXPECT(moved[0] == 5);
    EXPECT(moved[1] == 10);
    EXPECT(moved[2] == 15);
}

TEST_CASE(VectorTrivial, MoveAssign) {
    Vector<int> vector;
    vector.extend(Array{5, 10, 15});

    Vector<int> moved;
    moved = vull::move(vector);
    EXPECT(vector.empty());
    EXPECT(vector.size() == 0);
    EXPECT(vector.begin() == vector.end());
    EXPECT(!moved.empty());
    EXPECT(moved.size() == 3);
    EXPECT(moved[0] == 5);
    EXPECT(moved[1] == 10);
    EXPECT(moved[2] == 15);
}

TEST_CASE(VectorTrivial, MoveAssignSelf) {
    Vector<int> vector;
    vector.extend(Array{5, 10, 15});
    vector = vull::move(vector);
    EXPECT(!vector.empty());
    EXPECT(vector.size() == 3);
    EXPECT(vector[0] == 5);
    EXPECT(vector[1] == 10);
    EXPECT(vector[2] == 15);
}

TEST_CASE(VectorObject, Empty) {
    Vector<Foo> vector;
    EXPECT(vector.empty());
    EXPECT(vector.capacity() == 0);
    EXPECT(vector.size() == 0);
    EXPECT(vector.size_bytes() == 0);
    EXPECT(vector.begin() == vector.end());
}

TEST_CASE(VectorObject, EnsureCapacity) {
    Vector<Foo> vector;
    vector.ensure_capacity(16);
    EXPECT(vector.empty());
    EXPECT(vector.capacity() == 16);
    EXPECT(vector.size() == 0);
    EXPECT(vector.size_bytes() == 0);
    EXPECT(vector.begin() == vector.end());
}

TEST_CASE(VectorObject, EnsureSize) {
    int destruct_count = 0;
    {
        Vector<Foo> vector;
        vector.ensure_size(16, destruct_count);
        EXPECT(!vector.empty());
        EXPECT(vector.capacity() == 16);
        EXPECT(vector.size() == 16);
        EXPECT(vector.size_bytes() == 16 * sizeof(Foo));
        EXPECT(destruct_count == 0);

        unsigned count = 0;
        for ([[maybe_unused]] const auto &foo : vector) {
            count++;
        }
        EXPECT(count == vector.size());
        EXPECT(destruct_count == 0);

        count = 0;
        for (auto foo : vector) {
            count++;
        }
        EXPECT(count == vector.size());
        EXPECT(destruct_count == 16);
    }
    EXPECT(destruct_count == 32);
}

TEST_CASE(VectorObject, Emplace) {
    int destruct_count = 0;
    {
        Vector<Foo> vector;
        vector.emplace(destruct_count);
        vector.emplace(destruct_count);
        EXPECT(!vector.empty());
        EXPECT(vector.capacity() >= 2);
        EXPECT(vector.size() == 2);
        EXPECT(vector.size_bytes() == 2 * sizeof(Foo));
        EXPECT(destruct_count == 0);
    }
    EXPECT(destruct_count == 2);
}

TEST_CASE(VectorObject, Push) {
    int destruct_count = 0;
    {
        Foo foo(destruct_count);
        Vector<Foo> vector;
        vector.push(foo);
        vector.push(vull::move(foo));
        EXPECT(!vector.empty());
        EXPECT(vector.capacity() >= 2);
        EXPECT(vector.size() == 2);
        EXPECT(vector.size_bytes() == 2 * sizeof(Foo));
        EXPECT(destruct_count == 0);
    }
    EXPECT(destruct_count == 2);
}

TEST_CASE(VectorObject, Extend) {
    int destruct_count = 0;
    {
        Vector<Foo> vector;
        vector.ensure_size(3, destruct_count);

        Vector<Foo> extended;
        extended.extend(vector);
        EXPECT(!extended.empty());
        EXPECT(extended.capacity() >= 3);
        EXPECT(extended.size() == 3);

        extended.extend(vector);
        EXPECT(!extended.empty());
        EXPECT(extended.capacity() >= 6);
        EXPECT(extended.size() == 6);
    }
    EXPECT(destruct_count == 9);
}

TEST_CASE(VectorObject, PopTakeLast) {
    int destruct_count = 0;
    {
        Vector<Foo> vector;
        vector.ensure_size(3, destruct_count);
        vector.emplace(destruct_count);
        vector.pop();
        EXPECT(!vector.empty());
        EXPECT(vector.size() == 3);
        EXPECT(vector.size_bytes() == 3 * sizeof(Foo));
        EXPECT(destruct_count == 1);
        vector.take_last();
        vector.pop();
        vector.pop();
        EXPECT(vector.empty());
        EXPECT(vector.size() == 0);
        EXPECT(vector.size_bytes() == 0);
        EXPECT(destruct_count == 4);
    }
    EXPECT(destruct_count == 4);
}

TEST_CASE(VectorObject, Clear) {
    int destruct_count = 0;
    Vector<Foo> vector;
    vector.ensure_size(16, destruct_count);
    vector.clear();
    EXPECT(vector.empty());
    EXPECT(vector.capacity() == 0);
    EXPECT(vector.size() == 0);
    EXPECT(vector.size_bytes() == 0);
    EXPECT(vector.begin() == vector.end());
    EXPECT(destruct_count == 16);

    // Reuse vector.
    vector.emplace(destruct_count);
    EXPECT(!vector.empty());
    EXPECT(vector.capacity() >= 1);
    EXPECT(vector.size() == 1);
    EXPECT(vector.size_bytes() == sizeof(Foo));
    vector.clear();
    EXPECT(destruct_count == 17);
}

TEST_CASE(VectorObject, MoveConstruct) {
    int destruct_count = 0;
    Vector<Foo> vector;
    vector.ensure_size(16, destruct_count);

    Vector<Foo> moved(vull::move(vector));
    EXPECT(vector.empty());
    EXPECT(vector.size() == 0);
    EXPECT(vector.begin() == vector.end());
    EXPECT(!moved.empty());
    EXPECT(moved.size() == 16);

    EXPECT(destruct_count == 0);
    moved.clear();
    EXPECT(destruct_count == 16);
    vector.clear();
    EXPECT(destruct_count == 16);
}

TEST_CASE(VectorObject, MoveAssign) {
    int destruct_count = 0;
    Vector<Foo> vector;
    vector.ensure_size(16, destruct_count);

    Vector<Foo> moved;
    moved = vull::move(vector);
    EXPECT(vector.empty());
    EXPECT(vector.size() == 0);
    EXPECT(vector.begin() == vector.end());
    EXPECT(!moved.empty());
    EXPECT(moved.size() == 16);

    EXPECT(destruct_count == 0);
    moved.clear();
    EXPECT(destruct_count == 16);
    vector.clear();
    EXPECT(destruct_count == 16);
}

TEST_CASE(VectorObject, MoveAssignSelf) {
    int destruct_count = 0;
    Vector<Foo> vector;
    vector.ensure_size(16, destruct_count);
    vector = vull::move(vector);
    EXPECT(!vector.empty());
    EXPECT(vector.size() == 16);
    EXPECT(destruct_count == 0);
    vector.clear();
    EXPECT(destruct_count == 16);
}

// NOLINTEND(readability-container-size-empty)
