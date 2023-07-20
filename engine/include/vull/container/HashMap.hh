#pragma once

#include <vull/container/HashSet.hh>
#include <vull/container/MapEntry.hh> // IWYU pragma: export

namespace vull {

template <typename K, typename V>
class HashMap {
    using Entry = MapEntry<K, V>;
    HashSet<Entry> m_set;

public:
    void clear();
    bool ensure_capacity(size_t capacity);
    void rehash(size_t capacity);

    bool contains(const K &key) const;
    Optional<V &> get(const K &key);
    Optional<const V &> get(const K &key) const;
    bool set(const K &key, const V &value);
    bool set(const K &key, V &&value);
    bool set(K &&key, const V &value);
    bool set(K &&key, V &&value);
    V &operator[](const K &key);

    auto begin() { return m_set.begin(); }
    auto end() { return m_set.end(); }
    auto begin() const { return m_set.begin(); }
    auto end() const { return m_set.end(); }

    bool empty() const { return m_set.empty(); }
    size_t capacity() const { return m_set.capacity(); }
    size_t size() const { return m_set.size(); }
};

template <typename K, typename V>
void HashMap<K, V>::clear() {
    m_set.clear();
}

template <typename K, typename V>
bool HashMap<K, V>::ensure_capacity(size_t capacity) {
    return m_set.ensure_capacity(capacity);
}

template <typename K, typename V>
void HashMap<K, V>::rehash(size_t capacity) {
    m_set.rehash(capacity);
}

template <typename K, typename V>
bool HashMap<K, V>::contains(const K &key) const {
    return !!get(key);
}

template <typename K, typename V>
Optional<V &> HashMap<K, V>::get(const K &key) {
    auto entry = m_set.find_hash(hash_of(key), [&](Entry &other) {
        return key == other.key;
    });
    return entry ? Optional<V &>(entry->value) : vull::nullopt;
}

template <typename K, typename V>
Optional<const V &> HashMap<K, V>::get(const K &key) const {
    auto entry = m_set.find_hash(hash_of(key), [&](const Entry &other) {
        return key == other.key;
    });
    return entry ? Optional<const V &>(entry->value) : vull::nullopt;
}

template <typename K, typename V>
bool HashMap<K, V>::set(const K &key, const V &value) {
    return set(K(key), V(value));
}

template <typename K, typename V>
bool HashMap<K, V>::set(const K &key, V &&value) {
    return set(K(key), move(value));
}

template <typename K, typename V>
bool HashMap<K, V>::set(K &&key, const V &value) {
    return set(move(key), V(value));
}

template <typename K, typename V>
bool HashMap<K, V>::set(K &&key, V &&value) {
    if (auto existing = m_set.add({move(key), move(value)})) {
        existing->value = move(value);
        return false;
    }
    return true;
}

template <typename K, typename V>
V &HashMap<K, V>::operator[](const K &key) {
    if (!contains(key)) {
        set(key, {});
    }
    return *get(key);
}

} // namespace vull
