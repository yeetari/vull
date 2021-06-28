#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <limits>
#include <utility>

class Device;

class Fence {
    const Device *m_device{nullptr};
    VkFence m_fence{nullptr};

public:
    constexpr Fence() = default;
    Fence(const Device &device, bool signalled);
    Fence(const Fence &) = delete;
    Fence(Fence &&other) noexcept : m_device(other.m_device), m_fence(std::exchange(other.m_fence, nullptr)) {}
    ~Fence();

    Fence &operator=(const Fence &) = delete;
    Fence &operator=(Fence &&other) noexcept {
        m_device = other.m_device;
        m_fence = std::exchange(other.m_fence, nullptr);
        return *this;
    }

    void block(std::uint64_t timeout = std::numeric_limits<std::uint64_t>::max()) const;
    void reset() const;

    VkFence operator*() const { return m_fence; }
};
