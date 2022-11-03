#pragma once

#include <vull/support/Vector.hh>
#include <vull/ui/Font.hh>
#include <vull/vulkan/Allocation.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Context;

} // namespace vull::vk

namespace vull::ui {

class GpuFont : public Font {
    vk::Context &m_context;
    vk::Allocation m_allocation;
    Vector<vkb::Image> m_images;
    Vector<vkb::ImageView> m_image_views;
    float *m_image_data{nullptr};

public:
    GpuFont(vk::Context &context, Font &&font);
    GpuFont(const GpuFont &) = delete;
    GpuFont(GpuFont &&) = delete;
    ~GpuFont();

    GpuFont &operator=(const GpuFont &) = delete;
    GpuFont &operator=(GpuFont &&) = delete;

    void rasterise(uint32_t glyph_index, vkb::DescriptorSet descriptor_set, vkb::Sampler sampler);
};

} // namespace vull::ui
