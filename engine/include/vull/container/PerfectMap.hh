#pragma once

#include <vull/container/MapEntry.hh>
#include <vull/container/Vector.hh>
#include <vull/support/Optional.hh>
#include <vull/support/PerfectHasher.hh>

namespace vull {

// A minimal perfect hash function, read-only hash map.
template <typename K, typename V>
class PerfectMap {
    using Entry = MapEntry<K, V>;
    Vector<Entry> m_entries;
    PerfectHasher m_phf;

public:
    static PerfectMap from_entries(const Vector<Entry> &entries);
    bool contains(const K &key) const;
    Optional<V &> get(const K &key);
    Optional<const V &> get(const K &key) const;
};

template <typename K, typename V>
PerfectMap<K, V> PerfectMap<K, V>::from_entries(const Vector<Entry> &entries) {
    Vector<K> keys;
    keys.ensure_capacity(entries.size());
    for (const auto &[key, value] : entries) {
        keys.push(key);
    }

    PerfectMap map;
    map.m_phf.build(keys);
    map.m_entries.ensure_size(entries.size());
    for (const auto &[key, value] : entries) {
        map.m_entries[map.m_phf.hash(key)] = {key, value};
    }
    return map;
}

template <typename K, typename V>
bool PerfectMap<K, V>::contains(const K &key) const {
    // TODO: Would it be better to compare hashes instead?
    return m_entries[m_phf.hash(key)].key == key;
}

template <typename K, typename V>
Optional<V &> PerfectMap<K, V>::get(const K &key) {
    auto &entry = m_entries[m_phf.hash(key)];
    return entry.key == key ? entry.value : Optional<V &>();
}

template <typename K, typename V>
Optional<const V &> PerfectMap<K, V>::get(const K &key) const {
    const auto &entry = m_entries[m_phf.hash(key)];
    return entry.key == key ? entry.value : Optional<const V &>();
}

} // namespace vull
