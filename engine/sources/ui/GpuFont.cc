#include <vull/ui/GpuFont.hh>

#include <vull/support/Assert.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/ui/Font.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Vulkan.hh>

#include <string.h>

namespace vull::ui {
namespace {

constexpr size_t k_glyph_pixel_count = 128ull * 128ull;
constexpr uint32_t k_glyph_size = 128ull;

} // namespace

GpuFont::GpuFont(const Context &context, Font &&font) : Font(move(font)), m_context(context) {
    m_images.ensure_size(glyph_count());
    m_image_views.ensure_size(glyph_count());
    vk::MemoryRequirements memory_requirements{
        .size = static_cast<vk::DeviceSize>(glyph_count()) * k_glyph_pixel_count * sizeof(float),
        .memoryTypeBits = 0xffffffffu,
    };
    m_memory = context.allocate_memory(memory_requirements, MemoryType::HostVisible);
}

GpuFont::~GpuFont() {
    m_context.vkFreeMemory(m_memory);
    for (auto *image_view : m_image_views) {
        m_context.vkDestroyImageView(image_view);
    }
    for (auto *image : m_images) {
        m_context.vkDestroyImage(image);
    }
}

void GpuFont::rasterise(uint32_t glyph_index, vk::DescriptorSet descriptor_set, vk::Sampler sampler) {
    vk::ImageCreateInfo image_ci{
        .sType = vk::StructureType::ImageCreateInfo,
        .imageType = vk::ImageType::_2D,
        .format = vk::Format::R32Sfloat,
        .extent = {k_glyph_size, k_glyph_size, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCount::_1,
        // TODO: HostVisible staging buffer -> Optimal DeviceLocal image.
        .tiling = vk::ImageTiling::Linear,
        .usage = vk::ImageUsage::Sampled,
        .sharingMode = vk::SharingMode::Exclusive,
        .initialLayout = vk::ImageLayout::Undefined,
    };
    VULL_ENSURE(m_context.vkCreateImage(&image_ci, &m_images[glyph_index]) == vk::Result::Success);

    const auto memory_offset = static_cast<vk::DeviceSize>(glyph_index) * k_glyph_pixel_count * sizeof(float);
    VULL_ENSURE(m_context.vkBindImageMemory(m_images[glyph_index], m_memory, memory_offset) == vk::Result::Success);

    vk::ImageViewCreateInfo image_view_ci{
        .sType = vk::StructureType::ImageViewCreateInfo,
        .image = m_images[glyph_index],
        .viewType = vk::ImageViewType::_2D,
        .format = image_ci.format,
        .subresourceRange{
            .aspectMask = vk::ImageAspect::Color,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    VULL_ENSURE(m_context.vkCreateImageView(&image_view_ci, &m_image_views[glyph_index]) == vk::Result::Success);

    float *image_data;
    const auto memory_size = k_glyph_pixel_count * sizeof(float);
    m_context.vkMapMemory(m_memory, memory_offset, memory_size, 0, reinterpret_cast<void **>(&image_data));
    memset(image_data, 0, memory_size);
    Font::rasterise({image_data, 128 * 128}, glyph_index);
    m_context.vkUnmapMemory(m_memory);

    vk::DescriptorImageInfo image_info{
        .sampler = sampler,
        .imageView = m_image_views[glyph_index],
        .imageLayout = vk::ImageLayout::ShaderReadOnlyOptimal,
    };
    vk::WriteDescriptorSet descriptor_write{
        .sType = vk::StructureType::WriteDescriptorSet,
        .dstSet = descriptor_set,
        .dstBinding = 6, // TODO
        .dstArrayElement = glyph_index,
        .descriptorCount = 1,
        // TODO: Don't use combined image sampler.
        .descriptorType = vk::DescriptorType::CombinedImageSampler,
        .pImageInfo = &image_info,
    };
    m_context.vkUpdateDescriptorSets(1, &descriptor_write, 0, nullptr);
}

} // namespace vull::ui
