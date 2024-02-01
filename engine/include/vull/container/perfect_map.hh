#pragma once

#include <vull/container/map_entry.hh>
#include <vull/container/vector.hh>
#include <vull/support/optional.hh>
#include <vull/support/perfect_hasher.hh>

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
    return entry.key == key ? Optional<V &>(entry.value) : vull::nullopt;
}

template <typename K, typename V>
Optional<const V &> PerfectMap<K, V>::get(const K &key) const {
    const auto &entry = m_entries[m_phf.hash(key)];
    return entry.key == key ? Optional<const V &>(entry.value) : vull::nullopt;
}

} // namespace vull
