#pragma once

#include <vull/container/array.hh>
#include <vull/maths/common.hh>
#include <vull/support/hash.hh>
#include <vull/support/optional.hh>
#include <vull/support/utility.hh>

namespace vull {
namespace detail {

template <typename T>
constexpr bool is_equal(const T &lhs, const T &rhs) {
    return lhs == rhs;
}

} // namespace detail

template <typename T, hash_t HashFn(const T &) = &vull::hash_of<T>,
          bool EqualityFn(const T &, const T &) = &detail::is_equal<T>>
class HashSet {
    struct Bucket {
        Bucket *next{nullptr};
        AlignedStorage<T> storage;

        Bucket() = default;
        Bucket(const Bucket &) = delete;
        Bucket(Bucket &&) = delete;
        ~Bucket() {
            delete next;
            storage.release();
        }

        Bucket &operator=(const Bucket &) = delete;
        Bucket &operator=(Bucket &&) = delete;

        void append(T &&elem) {
            if (next != nullptr) {
                next->append(move(elem));
                return;
            }
            do_append(move(elem));
        }
        void do_append(T &&elem) {
            VULL_ASSERT(next == nullptr);
            next = new Bucket;
            next->storage.set(move(elem));
        }
    };

    template <bool IsConst>
    class Iterator {
        friend HashSet;

    private:
        Bucket *m_root_bucket;
        Bucket *m_bucket;
        Bucket *m_end_bucket;

        Iterator(Bucket *bucket, Bucket *end_bucket)
            : m_root_bucket(bucket), m_bucket(bucket), m_end_bucket(end_bucket) {}

        void skip_to_next() {
            if (m_bucket == nullptr) [[unlikely]] {
                return;
            }
            m_bucket = m_bucket->next;
            if (m_bucket == nullptr) {
                m_bucket = ++m_root_bucket;
                if (m_bucket == m_end_bucket) {
                    return;
                }
                skip_to_next();
            }
        }

    public:
        bool operator==(const Iterator &other) const { return m_bucket == other.m_bucket; }
        Iterator &operator++() {
            skip_to_next();
            return *this;
        }
        T &operator*() const { return m_bucket->storage.get(); }
    };

    Bucket *m_buckets{nullptr};
    size_t m_capacity{0};
    size_t m_size{0};

    void insert(T &&elem);

public:
    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;

    HashSet() = default;
    HashSet(const HashSet &) = delete;
    HashSet(HashSet &&);
    ~HashSet();

    HashSet &operator=(const HashSet &) = delete;
    HashSet &operator=(HashSet &&) = delete;

    void clear();
    bool ensure_capacity(size_t capacity);
    void rehash(size_t capacity);

    Optional<T &> add(const T &elem);
    Optional<T &> add(T &&elem);
    bool contains(const T &elem) const;
    template <typename EqualFn>
    Optional<T &> find_hash(hash_t hash, EqualFn equal_fn) const;

    iterator begin() { return ++iterator(m_buckets, m_buckets + m_capacity); }
    iterator end() { return {m_buckets + m_capacity, nullptr}; }
    const_iterator begin() const { return ++const_iterator(m_buckets, m_buckets + m_capacity); }
    const_iterator end() const { return {m_buckets + m_capacity, nullptr}; }

    bool empty() const { return m_size == 0; }
    size_t capacity() const { return m_capacity; }
    size_t size() const { return m_size; }
};

template <typename T, hash_t HashFn(const T &), bool EqualityFn(const T &, const T &)>
HashSet<T, HashFn, EqualityFn>::HashSet(HashSet &&other) {
    m_buckets = exchange(other.m_buckets, nullptr);
    m_capacity = exchange(other.m_capacity, 0u);
    m_size = exchange(other.m_size, 0u);
}

template <typename T, hash_t HashFn(const T &), bool EqualityFn(const T &, const T &)>
HashSet<T, HashFn, EqualityFn>::~HashSet() {
    clear();
}

template <typename T, hash_t HashFn(const T &), bool EqualityFn(const T &, const T &)>
void HashSet<T, HashFn, EqualityFn>::insert(T &&elem) {
    auto &bucket = m_buckets[HashFn(elem) % m_capacity];
    bucket.append(move(elem));
}

template <typename T, hash_t HashFn(const T &), bool EqualityFn(const T &, const T &)>
void HashSet<T, HashFn, EqualityFn>::clear() {
    m_size = 0;
    m_capacity = 0;
    delete[] exchange(m_buckets, nullptr);
}

template <typename T, hash_t HashFn(const T &), bool EqualityFn(const T &, const T &)>
bool HashSet<T, HashFn, EqualityFn>::ensure_capacity(size_t capacity) {
    if (capacity > m_capacity) {
        rehash(max(m_capacity * 2 + 1, capacity));
        return true;
    }
    return false;
}

template <typename T, hash_t HashFn(const T &), bool EqualityFn(const T &, const T &)>
void HashSet<T, HashFn, EqualityFn>::rehash(size_t capacity) {
    VULL_ASSERT(capacity >= m_size);
    auto *old_buckets = exchange(m_buckets, new Bucket[capacity]);
    auto old_capacity = exchange(m_capacity, capacity);

    // Default construct root bucket elements.
    if constexpr (!is_trivially_copyable<T> || !is_trivially_destructible<T>) {
        for (size_t i = 0; i < m_capacity; i++) {
            m_buckets[i].storage.emplace();
        }
    }

    // Move old elements;
    for (size_t i = 0; i < old_capacity; i++) {
        for (auto *bucket = old_buckets[i].next; bucket != nullptr; bucket = bucket->next) {
            insert(move(bucket->storage.get()));
        }
    }
    delete[] old_buckets;
}

template <typename T, hash_t HashFn(const T &), bool EqualityFn(const T &, const T &)>
Optional<T &> HashSet<T, HashFn, EqualityFn>::add(const T &elem) {
    return add(T(elem));
}

template <typename T, hash_t HashFn(const T &), bool EqualityFn(const T &, const T &)>
Optional<T &> HashSet<T, HashFn, EqualityFn>::add(T &&elem) {
    Bucket *bucket = nullptr;
    if (!empty()) {
        bucket = &m_buckets[HashFn(elem) % m_capacity];
        while (true) {
            if (bucket->next == nullptr) {
                break;
            }
            bucket = bucket->next;
            if (EqualityFn(elem, bucket->storage.get())) {
                return bucket->storage.get();
            }
        }
    }
    if (ensure_capacity(m_size + 1)) {
        // Rehash took place.
        insert(move(elem));
    } else {
        VULL_ASSERT(bucket != nullptr);
        bucket->do_append(move(elem));
    }
    m_size++;
    return {};
}

template <typename T, hash_t HashFn(const T &), bool EqualityFn(const T &, const T &)>
bool HashSet<T, HashFn, EqualityFn>::contains(const T &elem) const {
    return !!find_hash(HashFn(elem), [&](const T &other) {
        return EqualityFn(elem, other);
    });
}

template <typename T, hash_t HashFn(const T &), bool EqualityFn(const T &, const T &)>
template <typename EqualFn>
Optional<T &> HashSet<T, HashFn, EqualityFn>::find_hash(hash_t hash, EqualFn equal_fn) const {
    if (empty()) {
        return {};
    }
    auto &root_bucket = m_buckets[hash % m_capacity];
    for (auto *bucket = root_bucket.next; bucket != nullptr; bucket = bucket->next) {
        if (auto &elem = bucket->storage.get(); equal_fn(elem)) {
            return elem;
        }
    }
    return {};
}

} // namespace vull
