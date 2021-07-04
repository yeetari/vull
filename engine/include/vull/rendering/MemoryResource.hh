#pragma once

#include <vulkan/vulkan_core.h>

enum class MemoryUsage {
    GpuOnly,
    HostVisible,
    Transfer,
};

class MemoryResource {
protected:
    const MemoryUsage m_usage;
    VkDeviceMemory m_memory{nullptr};

public:
    explicit MemoryResource(MemoryUsage usage) : m_usage(usage) {}

    // TODO: reserve function.

    virtual void transfer(const void *data, VkDeviceSize size) = 0;
    template <typename T>
    void transfer(const T &data);
    template <typename T, template <typename> typename Container>
    void transfer(const Container<T> &data);
//    void upload() {}
};

template <typename T>
void MemoryResource::transfer(const T &data) {
    transfer(&data, sizeof(T));
}

template <typename T, template <typename> typename Container>
void MemoryResource::transfer(const Container<T> &data) {
    transfer(data.data(), data.size_bytes());
}
