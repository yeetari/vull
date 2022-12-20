#include <vull/ui/Renderer.hh>

#include <vull/maths/Vec.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Optional.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/ui/Font.hh>
#include <vull/ui/GpuFont.hh>
#include <vull/vulkan/Buffer.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/MemoryUsage.hh>
#include <vull/vulkan/Pipeline.hh>
#include <vull/vulkan/PipelineBuilder.hh>
#include <vull/vulkan/RenderGraph.hh>
#include <vull/vulkan/Swapchain.hh>
#include <vull/vulkan/Vulkan.hh>

#include <ft2build.h> // IWYU pragma: keep
// IWYU pragma: no_include "freetype/config/ftheader.h"

#include <freetype/freetype.h>
#include <freetype/fterrors.h>
#include <string.h>

namespace vull::ui {

Renderer::Renderer(vk::Context &context, vk::RenderGraph &render_graph, const vk::Swapchain &swapchain,
                   vk::ImageResource &swapchain_resource, const vk::Shader &vertex_shader,
                   const vk::Shader &fragment_shader)
    : m_context(context), m_swapchain(swapchain) {
    VULL_ENSURE(FT_Init_FreeType(&m_ft_library) == FT_Err_Ok);

    vkb::SamplerCreateInfo font_sampler_ci{
        .sType = vkb::StructureType::SamplerCreateInfo,
        .magFilter = vkb::Filter::Linear,
        .minFilter = vkb::Filter::Linear,
        .mipmapMode = vkb::SamplerMipmapMode::Linear,
        .addressModeU = vkb::SamplerAddressMode::ClampToEdge,
        .addressModeV = vkb::SamplerAddressMode::ClampToEdge,
        .addressModeW = vkb::SamplerAddressMode::ClampToEdge,
        .borderColor = vkb::BorderColor::FloatOpaqueWhite,
    };
    VULL_ENSURE(context.vkCreateSampler(&font_sampler_ci, &m_font_sampler) == vkb::Result::Success);

    Array set_bindings{
        vkb::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vkb::DescriptorType::CombinedImageSampler,
            .descriptorCount = 2000,
            .stageFlags = vkb::ShaderStage::Fragment,
        },
    };
    Array set_binding_flags{
        vkb::DescriptorBindingFlags::PartiallyBound,
    };
    vkb::DescriptorSetLayoutBindingFlagsCreateInfo set_binding_flags_ci{
        .sType = vkb::StructureType::DescriptorSetLayoutBindingFlagsCreateInfo,
        .bindingCount = set_bindings.size(),
        .pBindingFlags = set_binding_flags.data(),
    };
    vkb::DescriptorSetLayoutCreateInfo set_layout_ci{
        .sType = vkb::StructureType::DescriptorSetLayoutCreateInfo,
        .pNext = &set_binding_flags_ci,
        .flags = vkb::DescriptorSetLayoutCreateFlags::DescriptorBufferEXT,
        .bindingCount = set_bindings.size(),
        .pBindings = set_bindings.data(),
    };
    VULL_ENSURE(context.vkCreateDescriptorSetLayout(&set_layout_ci, &m_descriptor_set_layout) == vkb::Result::Success);

    vkb::DeviceSize descriptor_buffer_size;
    context.vkGetDescriptorSetLayoutSizeEXT(m_descriptor_set_layout, &descriptor_buffer_size);
    m_descriptor_buffer = context.create_buffer(
        descriptor_buffer_size, vkb::BufferUsage::SamplerDescriptorBufferEXT | vkb::BufferUsage::ShaderDeviceAddress,
        vk::MemoryUsage::HostToDevice);

    Vec2f swapchain_dimensions = swapchain.dimensions();
    Array specialization_map_entries{
        vkb::SpecializationMapEntry{
            .constantID = 0,
            .size = sizeof(float),
        },
        vkb::SpecializationMapEntry{
            .constantID = 1,
            .offset = sizeof(float),
            .size = sizeof(float),
        },
    };
    vkb::SpecializationInfo specialization_info{
        .mapEntryCount = specialization_map_entries.size(),
        .pMapEntries = specialization_map_entries.data(),
        .dataSize = sizeof(Vec2f),
        .pData = &swapchain_dimensions,
    };
    vkb::PushConstantRange push_constant_range{
        .stageFlags = vkb::ShaderStage::Vertex | vkb::ShaderStage::Fragment,
        .size = sizeof(vkb::DeviceAddress),
    };

    vkb::PipelineColorBlendAttachmentState blend_state{
        .blendEnable = true,
        .srcColorBlendFactor = vkb::BlendFactor::SrcAlpha,
        .dstColorBlendFactor = vkb::BlendFactor::OneMinusSrcAlpha,
        .colorBlendOp = vkb::BlendOp::Add,
        .srcAlphaBlendFactor = vkb::BlendFactor::One,
        .dstAlphaBlendFactor = vkb::BlendFactor::Zero,
        .alphaBlendOp = vkb::BlendOp::Add,
        .colorWriteMask =
            vkb::ColorComponent::R | vkb::ColorComponent::G | vkb::ColorComponent::B | vkb::ColorComponent::A,
    };
    m_pipeline = vk::PipelineBuilder()
                     .add_colour_attachment(vkb::Format::B8G8R8A8Unorm, blend_state)
                     .add_set_layout(m_descriptor_set_layout)
                     .add_shader(vertex_shader, specialization_info)
                     .add_shader(fragment_shader, specialization_info)
                     .set_push_constant_range(push_constant_range)
                     .set_topology(vkb::PrimitiveTopology::TriangleList)
                     .set_viewport(swapchain.extent_2D())
                     .build(m_context);

    auto &ui_data_resource = render_graph.add_storage_buffer("ui-data");
    auto &ui_pass = render_graph.add_graphics_pass("ui-pass");
    ui_pass.reads_from(ui_data_resource);
    ui_pass.writes_to(swapchain_resource);
    ui_pass.set_on_record([this, &swapchain_resource](vk::CommandBuffer &cmd_buf) {
        auto object_buffer =
            m_context.create_buffer(sizeof(float) + m_objects.size_bytes(), vkb::BufferUsage::ShaderDeviceAddress,
                                    vk::MemoryUsage::HostToDevice);
        memcpy(object_buffer.mapped<float>(), &m_global_scale, sizeof(float));
        memcpy(object_buffer.mapped<float>() + 1, m_objects.data(), m_objects.size_bytes());

        const auto buffer_address = object_buffer.device_address();
        cmd_buf.bind_associated_buffer(vull::move(object_buffer));
        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Graphics, m_descriptor_buffer, 0, 0);

        vkb::RenderingAttachmentInfo colour_write_attachment{
            .sType = vkb::StructureType::RenderingAttachmentInfo,
            .imageView = swapchain_resource.view(),
            .imageLayout = vkb::ImageLayout::ColorAttachmentOptimal,
            .loadOp = vkb::AttachmentLoadOp::Load,
            .storeOp = vkb::AttachmentStoreOp::Store,
        };
        vkb::RenderingInfo rendering_info{
            .sType = vkb::StructureType::RenderingInfo,
            .renderArea{
                .extent = m_swapchain.extent_2D(),
            },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colour_write_attachment,
        };
        cmd_buf.bind_pipeline(m_pipeline);
        cmd_buf.push_constants(vkb::ShaderStage::Vertex | vkb::ShaderStage::Fragment, sizeof(vkb::DeviceAddress),
                               &buffer_address);
        cmd_buf.begin_rendering(rendering_info);
        cmd_buf.draw(6, m_objects.size());
        cmd_buf.end_rendering();
        m_objects.clear();
    });
}

Renderer::~Renderer() {
    m_context.vkDestroyDescriptorSetLayout(m_descriptor_set_layout);
    m_context.vkDestroySampler(m_font_sampler);
    FT_Done_FreeType(m_ft_library);
}

GpuFont Renderer::load_font(StringView path, ssize_t size) {
    FT_Face face;
    VULL_ENSURE(FT_New_Face(m_ft_library, path.data(), 0, &face) == FT_Err_Ok);
    VULL_ENSURE(FT_Set_Char_Size(face, size * 64l, 0, 0, 0) == FT_Err_Ok);
    return {m_context, Font(face)};
}

void Renderer::set_global_scale(float global_scale) {
    m_global_scale = global_scale;
}

void Renderer::draw_rect(const Vec4f &colour, const Vec2f &position, const Vec2f &scale) {
    m_objects.push({
        .colour = colour,
        .position = position,
        .scale = scale,
        .type = ObjectType::Rect,
    });
}

void Renderer::draw_text(GpuFont &font, const Vec3f &colour, const Vec2f &position, StringView text) {
    float cursor_x = position.x();
    float cursor_y = position.y();
    for (auto [glyph_index, x_advance, y_advance, x_offset, y_offset] : font.shape(text)) {
        const auto &glyph = font.cached_glyph(glyph_index);
        if (!glyph) {
            font.rasterise(glyph_index, m_descriptor_buffer.mapped<uint8_t>(), m_font_sampler);
        }
        Vec2f glyph_position(cursor_x + static_cast<float>(x_offset) / 64.0f,
                             cursor_y + static_cast<float>(y_offset) / 64.0f);
        glyph_position += {glyph->disp_x, glyph->disp_y};
        m_objects.push({
            .colour = {colour.x(), colour.y(), colour.z(), 1.0f},
            .position = glyph_position,
            .scale = Vec2f(64.0f),
            .glyph_index = glyph_index,
            .type = ObjectType::TextGlyph,
        });
        cursor_x += static_cast<float>(x_advance) / 64.0f;
        cursor_y += static_cast<float>(y_advance) / 64.0f;
    }
}

} // namespace vull::ui
