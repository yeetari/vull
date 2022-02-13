#pragma once

#include <vull/support/Vector.hh>
#include <vull/ui/Font.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull {

class Context;

} // namespace vull

namespace vull::ui {

class GpuFont : public Font {
    const Context &m_context;
    vk::DeviceMemory m_memory{nullptr};
    Vector<vk::Image> m_images;
    Vector<vk::ImageView> m_image_views;

public:
    GpuFont(const Context &context, Font &&font);
    GpuFont(const GpuFont &) = delete;
    GpuFont(GpuFont &&) = delete;
    ~GpuFont();

    GpuFont &operator=(const GpuFont &) = delete;
    GpuFont &operator=(GpuFont &&) = delete;

    void rasterise(uint32_t glyph_index, vk::DescriptorSet descriptor_set, vk::Sampler sampler);
};

} // namespace vull::ui
