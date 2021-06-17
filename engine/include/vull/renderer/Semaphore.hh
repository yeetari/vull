#pragma once

#include <vulkan/vulkan_core.h>

#include <utility>

class Device;

class Semaphore {
    const Device *m_device{nullptr};
    VkSemaphore m_semaphore{nullptr};

public:
    constexpr Semaphore() = default;
    explicit Semaphore(const Device &device);
    Semaphore(const Semaphore &) = delete;
    Semaphore(Semaphore &&other) noexcept
        : m_device(other.m_device), m_semaphore(std::exchange(other.m_semaphore, nullptr)) {}
    ~Semaphore();

    Semaphore &operator=(const Semaphore &) = delete;
    Semaphore &operator=(Semaphore &&other) noexcept {
        m_device = other.m_device;
        m_semaphore = std::exchange(other.m_semaphore, nullptr);
        return *this;
    }

    VkSemaphore operator*() const { return m_semaphore; }
};
