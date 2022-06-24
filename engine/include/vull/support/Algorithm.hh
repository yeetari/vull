#pragma once

#include <vull/support/Utility.hh>

namespace vull {

// TODO: Does vull want to go down the route of an "iterator" or a "container" based algorithm and data structure API?
//       For example, the Vector::extend(const Container &) API was added instead of one taking in begin and end
//       iterators (since calculating the extension size of the vector would be tricky), with the thought that
//       slices/ranges could be passed in via creating a Span. However, this design makes it impossible to do something
//       like
//           vector.extend(vull::reverse_view(other_vector));
//       as a ViewAdapter<ReverseIterator> doesn't have the normal container functions like .data() and .size()

template <typename It>
class ReverseIterator {
    It m_it;

public:
    explicit ReverseIterator(It it) : m_it(it) {}

    bool operator!=(const ReverseIterator &other) const { return m_it != other.m_it; }

    auto &operator++();
    auto &operator*() const;
};

template <typename It>
ReverseIterator(It) -> ReverseIterator<It>;

template <typename It>
class ViewAdapter {
    It m_begin;
    It m_end;

public:
    ViewAdapter(It begin, It end) : m_begin(begin), m_end(end) {}

    It begin() const { return m_begin; }
    It end() const { return m_end; }
};

template <typename It>
ViewAdapter(It) -> ViewAdapter<It>;

template <typename It>
auto &ReverseIterator<It>::operator++() {
    --m_it;
    return *this;
}

template <typename It>
auto &ReverseIterator<It>::operator*() const {
    It it = m_it;
    --it;
    return *it;
}

template <typename Container>
auto reverse_view(Container &container) {
    return ViewAdapter{ReverseIterator(container.end()), ReverseIterator(container.begin())};
}

template <typename Container, typename T>
constexpr bool contains(const Container &container, const T &value) {
    for (const auto &elem : container) {
        if (elem == value) {
            return true;
        }
    }
    return false;
}

template <typename Container, typename GreaterThan, typename SizeType = decltype(declval<Container>().size())>
constexpr void sort(Container &container, GreaterThan gt) {
    if (container.size() == 0) {
        return;
    }
    auto gap = container.size();
    bool swapped = false;
    do {
        gap = (gap * 10) / 13;
        if (gap == 9 || gap == 10) {
            gap = 11;
        }
        if (gap < 1) {
            gap = 1;
        }
        swapped = false;
        for (SizeType i = 0; i < container.size() - gap; i++) {
            SizeType j = i + gap;
            if (gt(container[i], container[j])) {
                swap(container[i], container[j]);
                swapped = true;
            }
        }
    } while (gap > 1 || swapped);
}

} // namespace vull
