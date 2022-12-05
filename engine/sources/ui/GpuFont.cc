#include <vull/ui/GpuFont.hh>

#include <vull/support/Assert.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/ui/Font.hh>
#include <vull/vulkan/Allocation.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/MemoryUsage.hh>
#include <vull/vulkan/Vulkan.hh>

#include <string.h>

namespace vull::ui {
namespace {

constexpr size_t k_glyph_pixel_count = 64ull * 64ull;
constexpr uint32_t k_glyph_size = 64ull;

} // namespace

GpuFont::GpuFont(vk::Context &context, Font &&font) : Font(vull::move(font)), m_context(context) {
    m_images.ensure_size(glyph_count());
    m_image_views.ensure_size(glyph_count());
    vkb::MemoryRequirements memory_requirements{
        .size = static_cast<vkb::DeviceSize>(glyph_count()) * k_glyph_pixel_count * sizeof(float),
        .memoryTypeBits = 0xffffffffu,
    };
    m_allocation = context.allocate_memory(memory_requirements, vk::MemoryUsage::HostToDevice);
    m_image_data = static_cast<float *>(m_allocation.mapped_data());
}

GpuFont::~GpuFont() {
    for (auto *image_view : m_image_views) {
        m_context.vkDestroyImageView(image_view);
    }
    for (auto *image : m_images) {
        m_context.vkDestroyImage(image);
    }
}

void GpuFont::rasterise(uint32_t glyph_index, uint8_t *descriptor_data, vkb::Sampler sampler) {
    vkb::ImageCreateInfo image_ci{
        .sType = vkb::StructureType::ImageCreateInfo,
        .imageType = vkb::ImageType::_2D,
        .format = vkb::Format::R32Sfloat,
        .extent = {k_glyph_size, k_glyph_size, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vkb::SampleCount::_1,
        // TODO: HostVisible staging buffer -> Optimal DeviceLocal image.
        .tiling = vkb::ImageTiling::Linear,
        .usage = vkb::ImageUsage::Sampled,
        .sharingMode = vkb::SharingMode::Exclusive,
        .initialLayout = vkb::ImageLayout::Undefined,
    };
    VULL_ENSURE(m_context.vkCreateImage(&image_ci, &m_images[glyph_index]) == vkb::Result::Success);

    const auto memory_offset = static_cast<vkb::DeviceSize>(glyph_index) * k_glyph_pixel_count;
    VULL_ENSURE(m_context.vkBindImageMemory(m_images[glyph_index], m_allocation.info().memory,
                                            m_allocation.info().offset + memory_offset * sizeof(float)) ==
                vkb::Result::Success);

    vkb::ImageViewCreateInfo image_view_ci{
        .sType = vkb::StructureType::ImageViewCreateInfo,
        .image = m_images[glyph_index],
        .viewType = vkb::ImageViewType::_2D,
        .format = image_ci.format,
        .subresourceRange{
            .aspectMask = vkb::ImageAspect::Color,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    VULL_ENSURE(m_context.vkCreateImageView(&image_view_ci, &m_image_views[glyph_index]) == vkb::Result::Success);

    const auto memory_size = k_glyph_pixel_count * sizeof(float);
    memset(&m_image_data[memory_offset], 0, memory_size);
    Font::rasterise({&m_image_data[memory_offset], k_glyph_pixel_count}, glyph_index);

    vkb::DescriptorImageInfo image_info{
        .sampler = sampler,
        .imageView = m_image_views[glyph_index],
        .imageLayout = vkb::ImageLayout::ShaderReadOnlyOptimal,
    };
    vkb::DescriptorGetInfoEXT descriptor_info{
        .sType = vkb::StructureType::DescriptorGetInfoEXT,
        // TODO: Don't use combined image sampler.
        .type = vkb::DescriptorType::CombinedImageSampler,
        .data{
            .pCombinedImageSampler = &image_info,
        },
    };
    const auto descriptor_size = m_context.descriptor_size(descriptor_info.type);
    m_context.vkGetDescriptorEXT(&descriptor_info, descriptor_size, descriptor_data + glyph_index * descriptor_size);
}

} // namespace vull::ui
