#pragma once

#include <vull/graphics/Frame.hh> // IWYU pragma: keep
#include <vull/support/Vector.hh>

#include <stdint.h>

namespace vull::vk {

class Swapchain;

} // namespace vull::vk

namespace vull {

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

    Frame &next_frame();

    uint32_t frame_index() const { return m_frame_index; }
    uint32_t image_index() const { return m_image_index; }
};

} // namespace vull
