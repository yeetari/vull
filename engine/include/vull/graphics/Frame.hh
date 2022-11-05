#pragma once

#include <vull/support/HashMap.hh>
#include <vull/support/StringView.hh>
#include <vull/vulkan/Fence.hh>
#include <vull/vulkan/QueryPool.hh>
#include <vull/vulkan/Semaphore.hh>

namespace vull::vk {

class RenderGraph;
class Swapchain;

} // namespace vull::vk

namespace vull {

class Frame {
    vk::Fence m_fence;
    vk::QueryPool m_timestamp_pool;
    vk::Semaphore m_acquire_semaphore;
    vk::Semaphore m_present_semaphore;

public:
    explicit Frame(const vk::Context &context)
        : m_fence(context, true), m_timestamp_pool(context), m_acquire_semaphore(context),
          m_present_semaphore(context) {}

    HashMap<StringView, float> pass_times(const vk::RenderGraph &render_graph);

    const vk::Fence &fence() const { return m_fence; }
    const vk::QueryPool &timestamp_pool() { return m_timestamp_pool; }
    const vk::Semaphore &acquire_semaphore() const { return m_acquire_semaphore; }
    const vk::Semaphore &present_semaphore() const { return m_present_semaphore; }
};

} // namespace vull
