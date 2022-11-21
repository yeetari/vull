#pragma once

namespace vull {

template <typename MutexType>
class ScopedLock {
    MutexType *m_mutex;

public:
    explicit ScopedLock(MutexType &mutex) : m_mutex(&mutex) { mutex.lock(); }
    ScopedLock(const ScopedLock &) = delete;
    ScopedLock(ScopedLock &&) = delete;
    ~ScopedLock();

    ScopedLock &operator=(const ScopedLock &) = delete;
    ScopedLock &operator=(ScopedLock &&) = delete;

    void unlock();
};

template <typename MutexType>
ScopedLock(MutexType &) -> ScopedLock<MutexType>;

template <typename MutexType>
ScopedLock<MutexType>::~ScopedLock() {
    if (m_mutex != nullptr) {
        m_mutex->unlock();
    }
}

template <typename MutexType>
void ScopedLock<MutexType>::unlock() {
    m_mutex->unlock();
    m_mutex = nullptr;
}

} // namespace vull
