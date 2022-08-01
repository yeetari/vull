#pragma once

namespace vull {

template <typename MutexType>
class ScopedLocker {
    MutexType *m_mutex;

public:
    explicit ScopedLocker(MutexType &mutex) : m_mutex(&mutex) { mutex.lock(); }
    ScopedLocker(const ScopedLocker &) = delete;
    ScopedLocker(ScopedLocker &&) = delete;
    ~ScopedLocker() {
        if (m_mutex != nullptr) {
            m_mutex->unlock();
        }
    }

    ScopedLocker &operator=(const ScopedLocker &) = delete;
    ScopedLocker &operator=(ScopedLocker &&) = delete;

    void unlock() {
        m_mutex->unlock();
        m_mutex = nullptr;
    }
};

template <typename MutexType>
ScopedLocker(MutexType &) -> ScopedLocker<MutexType>;

} // namespace vull
