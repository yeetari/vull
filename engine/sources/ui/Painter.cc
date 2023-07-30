#include <vull/ui/Painter.hh>

#include <vull/container/Array.hh>
#include <vull/container/Vector.hh>
#include <vull/maths/Colour.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Optional.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Variant.hh>
#include <vull/ui/Font.hh>
#include <vull/ui/FontAtlas.hh>
#include <vull/ui/Units.hh>
#include <vull/vulkan/Buffer.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Image.hh>
#include <vull/vulkan/MemoryUsage.hh>
#include <vull/vulkan/Vulkan.hh>

#include <string.h>

namespace vull::ui {

struct Vertex {
    Vec2i position;
    Vec2f uv;
    Vec4f colour;
};

uint32_t Painter::get_texture_index(const vk::SampledImage &image) {
    for (uint32_t i = 0; i < m_bound_textures.size(); i++) {
        const auto &texture = m_bound_textures[i];
        if (texture.view == *image.view() && texture.sampler == image.sampler()) {
            return i;
        }
    }
    m_bound_textures.push(BoundTexture{*image.view(), image.sampler()});
    return m_bound_textures.size() - 1;
}

void Painter::bind_atlas(FontAtlas &atlas) {
    m_atlas = &atlas;
}

void Painter::paint_rect(LayoutPoint position, LayoutSize size, const Colour &colour) {
    m_commands.push(PaintCommand{
        .position = position.floor(),
        .size = size.ceil(),
        .variant{RectCommand{
            .colour = colour,
        }},
    });
}

void Painter::paint_image(LayoutPoint position, LayoutSize size, const vk::SampledImage &image) {
    m_commands.push(PaintCommand{
        .position = position.floor(),
        .size = size.ceil(),
        .variant{ImageCommand{
            .texture_index = get_texture_index(image),
        }},
    });
}

void Painter::paint_text(Font &font, LayoutPoint position, const Colour &colour, StringView text) {
    for (const auto [glyph_index, advance, offset] : font.shape(text)) {
        const auto glyph_info = m_atlas->ensure_glyph(font, glyph_index);
        const auto glyph_position = position + offset + LayoutDelta::from_int_pixels(glyph_info.bitmap_offset);
        const auto glyph_size = LayoutSize::from_int_pixels(glyph_info.size);
        m_commands.push(PaintCommand{
            .position = glyph_position.floor(),
            .size = glyph_size.ceil(),
            .variant{TextCommand{
                .colour = colour,
                .uv_a = static_cast<Vec2f>(glyph_info.atlas_offset) / m_atlas->extent(),
                .uv_c = static_cast<Vec2f>(glyph_info.atlas_offset + glyph_info.size) / m_atlas->extent(),
                .texture_index = get_texture_index(m_atlas->sampled_image()),
            }},
        });

        // Round the advance to the nearest pixel to avoid jittering issues.
        position += LayoutDelta::from_int_pixels(advance.round());
    }
}

void Painter::compile(vk::Context &context, vk::CommandBuffer &cmd_buf, Vec2u viewport_extent,
                      const vk::SampledImage &null_image) {
    const auto descriptor_size = context.descriptor_size(vkb::DescriptorType::CombinedImageSampler);
    auto descriptor_buffer =
        context.create_buffer(m_bound_textures.size() * descriptor_size, vkb::BufferUsage::SamplerDescriptorBufferEXT,
                              vk::MemoryUsage::HostToDevice);

    m_bound_textures[0] = BoundTexture{*null_image.view(), null_image.sampler()};

    auto *descriptor_data = descriptor_buffer.mapped<uint8_t>();
    for (const auto &texture : m_bound_textures) {
        vkb::DescriptorImageInfo image_info{
            .sampler = texture.sampler,
            .imageView = texture.view,
            .imageLayout = vkb::ImageLayout::ReadOnlyOptimal,
        };
        vkb::DescriptorGetInfoEXT descriptor_info{
            .sType = vkb::StructureType::DescriptorGetInfoEXT,
            .type = vkb::DescriptorType::CombinedImageSampler,
            .data{
                .pCombinedImageSampler = &image_info,
            },
        };
        context.vkGetDescriptorEXT(&descriptor_info, descriptor_size, descriptor_data);
        descriptor_data += descriptor_size;
    }

    // TODO: Sort commands by texture index to minimise draw calls.
    const auto vertex_count = m_commands.size() * 4;
    const auto index_count = m_commands.size() * 6;
    auto vertex_buffer = context.create_buffer(static_cast<vkb::DeviceSize>(vertex_count) * sizeof(Vertex),
                                               vkb::BufferUsage::VertexBuffer, vk::MemoryUsage::HostToDevice);
    auto index_buffer = context.create_buffer(static_cast<vkb::DeviceSize>(index_count) * sizeof(uint32_t),
                                              vkb::BufferUsage::IndexBuffer, vk::MemoryUsage::HostToDevice);
    auto *const vertex_data = vertex_buffer.mapped<Vertex>();
    auto *const index_data = index_buffer.mapped<uint32_t>();

    cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Graphics, descriptor_buffer, 0, 0);
    cmd_buf.bind_vertex_buffer(vertex_buffer);
    cmd_buf.bind_index_buffer(index_buffer, vkb::IndexType::Uint32);
    cmd_buf.bind_associated_buffer(vull::move(descriptor_buffer));
    cmd_buf.bind_associated_buffer(vull::move(vertex_buffer));
    cmd_buf.bind_associated_buffer(vull::move(index_buffer));

    // TODO: Variant visit.
    uint32_t first_index = 0;
    uint32_t index_offset = 0;
    uint32_t vertex_index = 0;
    uint32_t texture_index = UINT32_MAX;
    for (const auto &command : m_commands) {
        uint32_t next_texture_index = 0;
        if (auto image_command = command.variant.try_get<ImageCommand>()) {
            next_texture_index = image_command->texture_index;
        } else if (auto text_command = command.variant.try_get<TextCommand>()) {
            next_texture_index = text_command->texture_index;
        }

        if (texture_index != next_texture_index) {
            texture_index = next_texture_index;
            if (const auto count = index_offset - first_index; count != 0) {
                // NOLINTNEXTLINE
                cmd_buf.draw_indexed(count, 1, first_index);
                first_index = index_offset;
            }
            struct PushConstants {
                Vec2u viewport;
                uint32_t texture_index;
            } push_constants{
                .viewport = viewport_extent,
                .texture_index = texture_index,
            };
            cmd_buf.push_constants(vkb::ShaderStage::Vertex | vkb::ShaderStage::Fragment, push_constants);
        }

        const auto a = command.position;
        const auto c = command.position + command.size;
        Vec2i b(c.x(), a.y());
        Vec2i d(a.x(), c.y());

        Vec2f uv_a(0.0f);
        Vec2f uv_c(1.0f);
        if (auto text_command = command.variant.try_get<TextCommand>()) {
            uv_a = text_command->uv_a;
            uv_c = text_command->uv_c;
        }
        Vec2f uv_b(uv_c.x(), uv_a.y());
        Vec2f uv_d(uv_a.x(), uv_c.y());

        auto colour = Colour::white();
        if (auto rect_command = command.variant.try_get<RectCommand>()) {
            colour = rect_command->colour;
        } else if (auto text_command = command.variant.try_get<TextCommand>()) {
            colour = text_command->colour;
        }

        Array vertices{
            Vertex{
                .position = a,
                .uv = uv_a,
                .colour = colour.rgba(),
            },
            Vertex{
                .position = b,
                .uv = uv_b,
                .colour = colour.rgba(),
            },
            Vertex{
                .position = c,
                .uv = uv_c,
                .colour = colour.rgba(),
            },
            Vertex{
                .position = d,
                .uv = uv_d,
                .colour = colour.rgba(),
            },
        };
        memcpy(&vertex_data[vertex_index], vertices.data(), vertices.size_bytes());

        Array indices{vertex_index, vertex_index + 1, vertex_index + 2,
                      vertex_index, vertex_index + 2, vertex_index + 3};
        memcpy(&index_data[index_offset], indices.data(), indices.size_bytes());
        vertex_index += 4;
        index_offset += 6;
    }
    if (const auto count = index_offset - first_index; count != 0) {
        cmd_buf.draw_indexed(count, 1, first_index);
    }
}

} // namespace vull::ui
