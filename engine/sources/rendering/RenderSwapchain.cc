#include <vull/rendering/RenderSwapchain.hh>

#include <vull/vulkan/Swapchain.hh>

RenderSwapchain::RenderSwapchain(const Swapchain &swapchain)
    : RenderTexture(TextureType::Swapchain, MemoryUsage::GpuOnly), m_swapchain(swapchain) {
    set_extent(swapchain.extent());
    set_format(swapchain.format());
}

void RenderSwapchain::set_image_index(std::uint32_t image_index) {
    m_image_view = m_swapchain.image_views()[image_index];
}
