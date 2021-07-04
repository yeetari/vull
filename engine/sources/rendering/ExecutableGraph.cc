#include <vull/rendering/ExecutableGraph.hh>

#include <vull/rendering/RenderGraph.hh>
#include <vull/rendering/RenderStage.hh>
#include <vull/vulkan/Device.hh>
#include <vull/vulkan/Fence.hh>

#include <vulkan/vulkan_core.h>

ExecutableGraph::ExecutableGraph(const CompiledGraph *compiled_graph, const Device &device,
                                 std::uint32_t frame_queue_length)
    : m_compiled_graph(compiled_graph), m_device(device),
      m_frames(frame_queue_length, device, compiled_graph->stage_order().size()) {
    // TODO: Use dedicated transfer queue.
    for (std::uint32_t i = 0; i < m_device.queue_families().size(); i++) {
        const auto &family = m_device.queue_families()[i];
        if ((family.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0u && (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u) {
            vkGetDeviceQueue(*device, i, 0, &m_queue);
        }
    }
    ENSURE(m_queue != nullptr);
}

StagingBuffer ExecutableGraph::create_staging_buffer(const void *data, VkDeviceSize size) {
    VkBufferCreateInfo buffer_ci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkBuffer staging_buffer;
    ENSURE(vkCreateBuffer(*m_device, &buffer_ci, nullptr, &staging_buffer) == VK_SUCCESS);

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(*m_device, staging_buffer, &memory_requirements);
    VkDeviceMemory staging_memory =
        m_device.allocate_memory(memory_requirements, MemoryType::CpuToGpu, false, staging_buffer, nullptr);
    ENSURE(vkBindBufferMemory(*m_device, staging_buffer, staging_memory, 0) == VK_SUCCESS);

    void *staging_data;
    vkMapMemory(*m_device, staging_memory, 0, VK_WHOLE_SIZE, 0, &staging_data);
    std::memcpy(staging_data, data, size);
    vkUnmapMemory(*m_device, staging_memory);
    return {staging_buffer, staging_memory};
}

void ExecutableGraph::enqueue_buffer_transfer(const BufferTransfer &transfer) {
    m_buffer_transfer_queue.push(transfer);
}

void ExecutableGraph::start_frame(std::uint32_t frame_index) {
    m_frame_index = frame_index % m_frames.size();
    auto &frame = m_frames[m_frame_index];
    frame.execute_pending_deletions();
    frame.reset_pool();

    // Submit transfers now so that the GPU doesn't stall whilst waiting for the CPU to generate the rendering command
    // buffers.
    if (!m_buffer_transfer_queue.empty()) {
        auto *transfer_buffer = frame.transfer_buffer();
        VkCommandBufferBeginInfo transfer_buffer_bi{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(transfer_buffer, &transfer_buffer_bi);

        while (!m_buffer_transfer_queue.empty()) {
            auto transfer = m_buffer_transfer_queue.pop();
            VkBufferCopy region{
                .size = transfer.size,
            };
            vkCmdCopyBuffer(transfer_buffer, transfer.src.buffer(), transfer.dst, 1, &region);
            frame.enqueue_deletion(transfer.src);
        }

        VkMemoryBarrier barrier{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        };
        vkCmdPipelineBarrier(transfer_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 1,
                             &barrier, 0, nullptr, 0, nullptr);
        vkEndCommandBuffer(transfer_buffer);

        VkSubmitInfo transfer_si{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &transfer_buffer,
        };
        vkQueueSubmit(m_queue, 1, &transfer_si, nullptr);
    }

    for (std::uint32_t i = 0; const auto *stage : m_compiled_graph->stage_order()) {
        stage->start_recording(frame.command_buffer(i++));
    }
}

void ExecutableGraph::submit_frame(const Fence &signal_fence) {
    m_command_buffers.clear();
    m_submit_infos.clear();
    for (const auto *stage : m_compiled_graph->stage_order()) {
        auto *cmd_buf = stage->stop_recording();
        const auto &signal_semaphores = stage->signal_semaphores(m_frame_index);
        const auto &wait_semaphores = stage->wait_semaphores(m_frame_index);
        m_submit_infos.push(VkSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = wait_semaphores.size(),
            .pWaitSemaphores = wait_semaphores.data(),
            .pWaitDstStageMask = stage->wait_stages().data(),
            .commandBufferCount = 1,
            .pCommandBuffers = &m_command_buffers.emplace(cmd_buf),
            .signalSemaphoreCount = signal_semaphores.size(),
            .pSignalSemaphores = signal_semaphores.data(),
        });
    }
    vkQueueSubmit(m_queue, m_submit_infos.size(), m_submit_infos.data(), *signal_fence);
}
