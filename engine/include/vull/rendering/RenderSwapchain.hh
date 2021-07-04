#pragma once

#include <vull/rendering/RenderTexture.hh>

#include <cstdint>

class Swapchain;

class RenderSwapchain : public RenderTexture {
    const Swapchain &m_swapchain;

public:
    explicit RenderSwapchain(const Swapchain &swapchain);

    void set_image_index(std::uint32_t image_index);
};
