#pragma once

#include <vull/support/hash.hh>

namespace vull {

template <typename K, typename V>
struct MapEntry {
    K key;
    V value;

    bool operator==(const MapEntry &other) const { return key == other.key; }
};

template <typename K, typename V>
struct Hash<MapEntry<K, V>> {
    hash_t operator()(const MapEntry<K, V> &entry) const { return hash_of(entry.key); }
    hash_t operator()(const MapEntry<K, V> &entry, hash_t seed) const { return hash_of(entry.key, seed); }
};

} // namespace vull
