#pragma once

#include <vull/container/HashMap.hh>
#include <vull/container/Vector.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/vulkan/Fence.hh>
#include <vull/vulkan/Semaphore.hh>

#include <stdint.h>

namespace vull::vk {

class Context;
class RenderGraph;
class Swapchain;

} // namespace vull::vk

namespace vull {

class StringView;

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

class FramePacer {
    const vk::Swapchain &m_swapchain;
    Vector<Frame> m_frames;
    uint32_t m_frame_index{0};
    uint32_t m_image_index{0};

public:
    FramePacer(const vk::Swapchain &swapchain, uint32_t queue_length);
    FramePacer(const FramePacer &) = delete;
    FramePacer(FramePacer &&) = delete;
    ~FramePacer() = default;

    FramePacer &operator=(const FramePacer &) = delete;
    FramePacer &operator=(FramePacer &&) = delete;

    Frame &request_frame();

    uint32_t queue_length() const { return m_frames.size(); }
    uint32_t frame_index() const { return m_frame_index; }
    uint32_t image_index() const { return m_image_index; }
};

} // namespace vull
