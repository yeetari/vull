#pragma once

#include <vull/support/utility.hh>

namespace vull {

// TODO: Use ranges more.

template <typename It>
class Range {
    It m_begin;
    It m_end;

public:
    constexpr Range(It begin, It end) : m_begin(begin), m_end(end) {}

    It begin() const { return m_begin; }
    It end() const { return m_end; }
};

template <typename It>
Range(It) -> Range<It>;

template <typename It>
constexpr auto make_range(It begin, It end) {
    return Range(begin, end);
}

template <typename Container>
class BackInserterIterator {
    Container *m_container;

public:
    explicit BackInserterIterator(Container &container) : m_container(&container) {}

    BackInserterIterator &operator=(const auto &value) {
        m_container->push(value);
        return *this;
    }
    BackInserterIterator &operator=(auto &&value) {
        m_container->push(vull::move(value));
        return *this;
    }

    BackInserterIterator &operator*() { return *this; }
    BackInserterIterator &operator++() { return *this; }
    BackInserterIterator &operator++(int) { return *this; }
};

template <typename Container>
BackInserterIterator(Container) -> BackInserterIterator<Container>;

template <typename Container>
auto back_inserter(Container &container) {
    return BackInserterIterator(container);
}

template <typename It>
class ReverseIterator {
    It m_it;

public:
    explicit ReverseIterator(It it) : m_it(it) {}

    bool operator==(const ReverseIterator &other) const { return m_it == other.m_it; }

    auto &operator++();
    auto &operator*() const;
};

template <typename It>
ReverseIterator(It) -> ReverseIterator<It>;

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
    return make_range(ReverseIterator(container.end()), ReverseIterator(container.begin()));
}

template <typename Container, typename SizeType>
auto slice(Container &container, SizeType first, SizeType last = 0) {
    last = last != 0 ? last : container.size();
    if constexpr (requires(decltype(container.begin()) it) {
                      it + 0;
                      it - 0;
                  }) {
        return make_range(container.begin() + first, container.end() - (container.size() - last));
    }

    auto begin = container.begin();
    for (SizeType i = 0; i < first; i++) {
        ++begin;
    }

    auto end = container.end();
    for (SizeType i = 0; i < container.size() - last; i++) {
        --end;
    }
    return make_range(begin, end);
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

template <typename InputIt, typename OutputIt>
constexpr OutputIt copy(InputIt first, InputIt last, OutputIt d_first) {
    for (; first != last; ++first, ++d_first) {
        *d_first = *first;
    }
    return d_first;
}

template <typename It, typename T>
constexpr It find(It first, It last, const T &value) {
    for (; first != last; ++first) {
        if (*first == value) {
            return first;
        }
    }
    return last;
}

template <typename It, typename Predicate>
constexpr It find_if(It first, It last, Predicate predicate) {
    for (; first != last; ++first) {
        if (predicate(*first)) {
            return first;
        }
    }
    return last;
}

template <typename It>
constexpr void reverse(It first, It last) {
    while (first != last && first != --last) {
        swap(*first++, *last);
    }
}

template <typename It>
constexpr It rotate(It first, It middle, It last) {
    if (first == middle) {
        return last;
    }

    if (middle == last) {
        return first;
    }

    // TODO: Support other random access iterators here.
    if constexpr (is_ptr<It>) {
        reverse(first, middle);
        reverse(middle, last);
        reverse(first, last);
        return first + (last - middle);
    }

    It write = first;
    It next_read = first;
    for (It read = middle; read != last; ++write, ++read) {
        if (write == next_read) {
            next_read = read;
        }
        swap(*write, *read);
    }

    rotate(write, next_read, last);
    return write;
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
