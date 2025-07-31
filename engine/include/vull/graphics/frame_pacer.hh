#pragma once

#include <vull/container/hash_map.hh>
#include <vull/container/vector.hh>
#include <vull/maths/vec.hh>
#include <vull/platform/event.hh>
#include <vull/platform/thread.hh>
#include <vull/support/atomic.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh> // IWYU pragma: keep
#include <vull/tasklet/future.hh>     // IWYU pragma: keep
#include <vull/tasklet/promise.hh>

#include <stdint.h>

namespace vull::vk {

class Context;
class Image;
class RenderGraph;
class Semaphore;
class Swapchain;

} // namespace vull::vk

namespace vull {

struct FrameInfo {
    const vk::Semaphore &acquire_semaphore;
    const vk::Semaphore &present_semaphore;
    const vk::Image &swapchain_image;
    vk::RenderGraph &graph;
    HashMap<String, float> pass_times;
    uint32_t frame_index;
};

class FramePacer {
    vk::Context &m_context;
    vk::Swapchain &m_swapchain;
    Vector<tasklet::Future<void>> m_frame_futures;
    Vector<vk::Semaphore> m_acquire_semaphores;
    Vector<vk::Semaphore> m_present_semaphores;
    Vector<UniquePtr<vk::RenderGraph>> m_render_graphs;
    platform::Event m_recorded_event;
    platform::Thread m_thread;
    tasklet::Promise<FrameInfo> m_promise;
    Vec2u m_window_extent;
    uint32_t m_frame_index{0};
    Atomic<bool> m_running{true};

public:
    FramePacer(vk::Swapchain &swapchain, uint32_t queue_length);
    FramePacer(const FramePacer &) = delete;
    FramePacer(FramePacer &&) = delete;
    ~FramePacer();

    FramePacer &operator=(const FramePacer &) = delete;
    FramePacer &operator=(FramePacer &&) = delete;

    FrameInfo acquire_frame(Vec2u window_extent);
    void submit_frame(tasklet::Future<void> &&future);
    uint32_t queue_length() const { return m_frame_futures.size(); }
};

} // namespace vull
