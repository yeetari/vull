#pragma once

#include <vull/container/Vector.hh>
#include <vull/support/Algorithm.hh>
#include <vull/support/Hash.hh>
#include <vull/support/Span.hh>
#include <vull/support/String.hh>

#include <stdint.h>

namespace vull {

class PerfectHasher {
    Vector<int32_t> m_seeds;

public:
    PerfectHasher() = default;
    PerfectHasher(Vector<int32_t> &&seeds) : m_seeds(move(seeds)) {}

    template <typename T>
    void build(const Vector<T> &keys);
    template <typename T>
    uint32_t hash(const T &key) const;

    Vector<int32_t> &seeds() { return m_seeds; }
    const Vector<int32_t> &seeds() const { return m_seeds; }
};

template <typename T>
void PerfectHasher::build(const Vector<T> &keys) {
    // TODO: This will probably break if T is already a reference (e.g. if you had a PerfectMap<Foo &>)
    struct Bucket : Vector<const T &> {
        uint32_t original_index;
    };

    Vector<Bucket> buckets(keys.size());
    for (uint32_t i = 0; i < keys.size(); i++) {
        buckets[i].original_index = i;
    }

    for (const auto &key : keys) {
        const auto hash = hash_of(key) % buckets.size();
        buckets[hash].push(key);
    }

    m_seeds.ensure_size(keys.size());
    vull::sort(buckets, [](const auto &lhs, const auto &rhs) {
        return lhs.size() < rhs.size();
    });

    uint32_t bucket_index = 0;
    Vector<bool> occupied(buckets.size());
    for (; bucket_index < buckets.size(); bucket_index++) {
        auto &bucket = buckets[bucket_index];
        if (bucket.size() == 1) {
            // The remaining buckets have no collisions.
            break;
        }

        for (uint32_t seed = 1;; seed++) {
            // TODO: Handle failure gracefully.
            VULL_ENSURE(seed < INT32_MAX);

            bool bucket_complete = true;
            Vector<uint32_t> occupied_bucket;
            for (const auto &key : bucket) {
                const auto hash = hash_of<T>(key, seed) % buckets.size();
                if (occupied[hash] || vull::contains(occupied_bucket, hash)) {
                    bucket_complete = false;
                    break;
                }
                occupied_bucket.push(hash);
            }

            if (bucket_complete) {
                for (uint32_t hash : occupied_bucket) {
                    occupied[hash] = true;
                }
                m_seeds[bucket.original_index] = static_cast<int32_t>(seed);
                break;
            }
        }
    }

    uint32_t occupied_index = 0;
    for (; bucket_index < buckets.size(); bucket_index++) {
        auto &bucket = buckets[bucket_index];
        if (bucket.empty()) {
            break;
        }
        while (occupied[occupied_index]) {
            occupied_index++;
        }
        occupied[occupied_index] = true;
        m_seeds[bucket.original_index] = -static_cast<int32_t>(occupied_index) - 1;
    }
}

template <typename T>
uint32_t PerfectHasher::hash(const T &key) const {
    const auto seed = m_seeds[hash_of(key) % m_seeds.size()];
    if (seed < 0) {
        VULL_ASSERT(-seed - 1 >= 0);
        return static_cast<uint32_t>(-seed - 1);
    }
    return hash_of(key, static_cast<uint32_t>(seed)) % m_seeds.size();
}

} // namespace vull
