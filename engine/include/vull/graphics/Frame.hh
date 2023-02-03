#pragma once

#include <vull/support/HashMap.hh>
#include <vull/support/StringView.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/vulkan/Fence.hh>
#include <vull/vulkan/RenderGraphDefs.hh>
#include <vull/vulkan/Semaphore.hh>

namespace vull::vk {

class RenderGraph;
class Swapchain;

} // namespace vull::vk

namespace vull {

class Frame {
    vk::Fence m_fence;
    vk::Semaphore m_acquire_semaphore;
    vk::Semaphore m_present_semaphore;
    UniquePtr<vk::RenderGraph> m_render_graph;

public:
    explicit Frame(const vk::Context &context);
    Frame(const Frame &) = delete;
    Frame(Frame &&) = default;
    ~Frame();

    Frame &operator=(const Frame &) = delete;
    Frame &operator=(Frame &&) = delete;

    HashMap<StringView, float> pass_times();
    vk::RenderGraph &new_graph(vk::Context &context);

    const vk::Fence &fence() const { return m_fence; }
    const vk::Semaphore &acquire_semaphore() const { return m_acquire_semaphore; }
    const vk::Semaphore &present_semaphore() const { return m_present_semaphore; }
};

} // namespace vull
