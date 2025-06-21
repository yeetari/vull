#include <vull/support/utility.hh>

#include <stddef.h>

namespace vull::test {

class MoveTester {
    size_t *m_destruct_count{nullptr};

public:
    MoveTester() = default;
    explicit MoveTester(size_t &destruct_count) : m_destruct_count(&destruct_count) {}
    MoveTester(const MoveTester &) = default;
    MoveTester(MoveTester &&other) : m_destruct_count(vull::exchange(other.m_destruct_count, nullptr)) {}
    ~MoveTester();

    MoveTester &operator=(const MoveTester &) = default;
    MoveTester &operator=(MoveTester &&);

    bool is_empty() const { return m_destruct_count == nullptr; }
};

inline MoveTester::~MoveTester() {
    if (m_destruct_count != nullptr) {
        (*m_destruct_count)++;
    }
    m_destruct_count = nullptr;
}

inline MoveTester &MoveTester::operator=(MoveTester &&other) {
    MoveTester moved(vull::move(other));
    vull::swap(m_destruct_count, moved.m_destruct_count);
    return *this;
}

} // namespace vull::test
