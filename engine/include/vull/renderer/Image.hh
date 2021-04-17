#pragma once

#include <vulkan/vulkan_core.h>

#include <utility>

class Device;

class Image {
    const Device *m_device{nullptr};
    VkImage m_image{nullptr};
    VkDeviceMemory m_memory{nullptr};

public:
    constexpr Image() = default;
    Image(const Device *device, VkImage image, VkDeviceMemory memory)
        : m_device(device), m_image(image), m_memory(memory) {}
    Image(const Image &) = delete;
    Image(Image &&) = delete;
    ~Image();

    Image &operator=(const Image &) = delete;
    Image &operator=(Image &&other) noexcept {
        m_device = other.m_device;
        m_image = std::exchange(other.m_image, nullptr);
        m_memory = std::exchange(other.m_memory, nullptr);
        return *this;
    }

    VkImage operator*() const { return m_image; }
};
