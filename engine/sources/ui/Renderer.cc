#include <vull/ui/Renderer.hh>

#include <vull/maths/Vec.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Optional.hh>
#include <vull/ui/Font.hh>
#include <vull/ui/GpuFont.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Vulkan.hh>

#include <ft2build.h> // IWYU pragma: keep
// IWYU pragma: no_include "freetype/config/ftheader.h"

#include <freetype/freetype.h>
#include <freetype/fterrors.h>

namespace vull::ui {

Renderer::Renderer(const Context &context, vk::DescriptorSet descriptor_set)
    : m_context(context), m_descriptor_set(descriptor_set) {
    VULL_ENSURE(FT_Init_FreeType(&m_ft_library) == FT_Err_Ok);

    // TODO: Dynamic resizing.
    vk::BufferCreateInfo object_buffer_ci{
        .sType = vk::StructureType::BufferCreateInfo,
        .size = sizeof(Object) * 1000,
        .usage = vk::BufferUsage::StorageBuffer,
        .sharingMode = vk::SharingMode::Exclusive,
    };
    VULL_ENSURE(context.vkCreateBuffer(&object_buffer_ci, &m_object_buffer) == vk::Result::Success);

    vk::MemoryRequirements object_buffer_requirements{};
    context.vkGetBufferMemoryRequirements(m_object_buffer, &object_buffer_requirements);
    m_object_buffer_memory = context.allocate_memory(object_buffer_requirements, MemoryType::HostVisible);
    VULL_ENSURE(context.vkBindBufferMemory(m_object_buffer, m_object_buffer_memory, 0) == vk::Result::Success);

    vk::SamplerCreateInfo font_sampler_ci{
        .sType = vk::StructureType::SamplerCreateInfo,
        .magFilter = vk::Filter::Linear,
        .minFilter = vk::Filter::Linear,
        .mipmapMode = vk::SamplerMipmapMode::Linear,
        .addressModeU = vk::SamplerAddressMode::ClampToEdge,
        .addressModeV = vk::SamplerAddressMode::ClampToEdge,
        .addressModeW = vk::SamplerAddressMode::ClampToEdge,
        .borderColor = vk::BorderColor::FloatOpaqueWhite,
    };
    VULL_ENSURE(context.vkCreateSampler(&font_sampler_ci, &m_font_sampler) == vk::Result::Success);

    vk::DescriptorBufferInfo object_buffer_info{
        .buffer = m_object_buffer,
        .range = vk::VK_WHOLE_SIZE,
    };
    vk::WriteDescriptorSet object_buffer_descriptor_write{
        .sType = vk::StructureType::WriteDescriptorSet,
        .dstSet = descriptor_set,
        .dstBinding = 5, // TODO
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::StorageBuffer,
        .pBufferInfo = &object_buffer_info,
    };
    context.vkUpdateDescriptorSets(1, &object_buffer_descriptor_write, 0, nullptr);

    context.vkMapMemory(m_object_buffer_memory, 0, vk::VK_WHOLE_SIZE, 0, reinterpret_cast<void **>(&m_objects));
}

Renderer::~Renderer() {
    m_context.vkDestroySampler(m_font_sampler);
    m_context.vkFreeMemory(m_object_buffer_memory);
    m_context.vkDestroyBuffer(m_object_buffer);
    FT_Done_FreeType(m_ft_library);
}

GpuFont Renderer::load_font(const char *path, ssize_t size) {
    FT_Face face;
    VULL_ENSURE(FT_New_Face(m_ft_library, path, 0, &face) == FT_Err_Ok);
    VULL_ENSURE(FT_Set_Char_Size(face, size * 64l, 0, 0, 0) == FT_Err_Ok);
    return {m_context, Font(face)};
}

void Renderer::new_frame() {
    m_object_index = 0;
}

void Renderer::draw_text(GpuFont &font, const Vec2u &position, const char *text) {
    auto cursor_x = static_cast<int32_t>(position.x());
    auto cursor_y = static_cast<int32_t>(position.y());
    for (auto [glyph_index, x_advance, y_advance, x_offset, y_offset] : font.shape(text)) {
        const auto &glyph = font.cached_glyph(glyph_index);
        if (!glyph) {
            font.rasterise(glyph_index, m_descriptor_set, m_font_sampler);
        }
        Vec2f glyph_position(static_cast<float>(cursor_x + x_offset / 64),  // NOLINT
                             static_cast<float>(cursor_y + y_offset / 64)); // NOLINT
        glyph_position += {glyph->disp_x, glyph->disp_y};
        m_objects[m_object_index++] = {
            .position = glyph_position,
            .glyph_index = glyph_index,
        };
        cursor_x += x_advance / 64;
        cursor_y += y_advance / 64;
    }
}

} // namespace vull::ui
