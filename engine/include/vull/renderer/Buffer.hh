#pragma once

#include <vulkan/vulkan_core.h>

#include <utility>

class Device;

class Buffer {
    const Device *m_device{nullptr};
    VkBuffer m_buffer{nullptr};
    VkDeviceMemory m_memory{nullptr};

public:
    constexpr Buffer() = default;
    Buffer(const Device *device, VkBuffer buffer, VkDeviceMemory memory)
        : m_device(device), m_buffer(buffer), m_memory(memory) {}
    Buffer(const Buffer &) = delete;
    Buffer(Buffer &&) = delete;
    ~Buffer();

    Buffer &operator=(const Buffer &) = delete;
    Buffer &operator=(Buffer &&other) noexcept {
        m_device = other.m_device;
        m_buffer = std::exchange(other.m_buffer, nullptr);
        m_memory = std::exchange(other.m_memory, nullptr);
        return *this;
    }

    void *map_memory();
    void unmap_memory();

    VkBuffer operator*() const { return m_buffer; }
};
