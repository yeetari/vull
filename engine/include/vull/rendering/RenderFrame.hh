#pragma once

#include <vull/support/Vector.hh>

#include <vulkan/vulkan_core.h>

class Device;

class StagingBuffer {
    const VkBuffer m_buffer;
    const VkDeviceMemory m_memory;

public:
    StagingBuffer(VkBuffer buffer, VkDeviceMemory memory) : m_buffer(buffer), m_memory(memory) {}

    VkBuffer buffer() const { return m_buffer; }
    VkDeviceMemory memory() const { return m_memory; }
};

class RenderFrame {
    const Device &m_device;
    VkCommandPool m_command_pool{nullptr};
    VkCommandBuffer m_transfer_buffer{nullptr};
    Vector<VkCommandBuffer> m_command_buffers;
    Vector<StagingBuffer> m_staging_buffer_deletion_queue;

public:
    RenderFrame(const Device &device, std::uint32_t stage_count);
    RenderFrame(const RenderFrame &) = delete;
    RenderFrame(RenderFrame &&other) noexcept
        : m_device(other.m_device), m_command_pool(std::exchange(other.m_command_pool, nullptr)),
          m_transfer_buffer(std::exchange(other.m_transfer_buffer, nullptr)),
          m_command_buffers(std::move(other.m_command_buffers)),
          m_staging_buffer_deletion_queue(std::move(other.m_staging_buffer_deletion_queue)) {}
    ~RenderFrame();

    RenderFrame &operator=(const RenderFrame &) = delete;
    RenderFrame &operator=(RenderFrame &&) = delete;

    void enqueue_deletion(const StagingBuffer &staging_buffer);
    void execute_pending_deletions();
    void reset_pool();

    VkCommandBuffer command_buffer(std::uint32_t index) const { return m_command_buffers[index]; }
    VkCommandBuffer transfer_buffer() const { return m_transfer_buffer; }
};
