#pragma once

#include <vull/rendering/RenderFrame.hh>
#include <vull/support/Vector.hh>

#include <cstdint>

class CompiledGraph;
class Device;
class Fence;

struct BufferTransfer {
    StagingBuffer src;
    VkBuffer dst;
    VkDeviceSize size;
};

class ExecutableGraph {
    const CompiledGraph *const m_compiled_graph;
    const Device &m_device;
    Vector<RenderFrame> m_frames;
    std::uint32_t m_frame_index{0};

    VkQueue m_queue{nullptr};

    Vector<BufferTransfer> m_buffer_transfer_queue;

    Vector<VkCommandBuffer> m_command_buffers;
    Vector<VkSubmitInfo> m_submit_infos;

public:
    ExecutableGraph(const CompiledGraph *compiled_graph, const Device &device, std::uint32_t frame_queue_length);
    ExecutableGraph(const ExecutableGraph &) = delete;
    ExecutableGraph(ExecutableGraph &&) = delete;
    ~ExecutableGraph() = default;

    ExecutableGraph &operator=(const ExecutableGraph &) = delete;
    ExecutableGraph &operator=(ExecutableGraph &&) = delete;

    StagingBuffer create_staging_buffer(const void *data, VkDeviceSize size);
    void enqueue_buffer_transfer(const BufferTransfer &transfer);

    void start_frame(std::uint32_t frame_index);
    void submit_frame(const Fence &signal_fence);

    std::uint32_t frame_queue_length() const { return m_frames.size(); }
};
