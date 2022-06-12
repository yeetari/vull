#include <vull/ui/Renderer.hh>

#include <vull/maths/Vec.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Optional.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>
#include <vull/ui/Font.hh>
#include <vull/ui/GpuFont.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/RenderGraph.hh>
#include <vull/vulkan/Swapchain.hh>
#include <vull/vulkan/Vulkan.hh>

#include <ft2build.h> // IWYU pragma: keep
// IWYU pragma: no_include "freetype/config/ftheader.h"

#include <freetype/freetype.h>
#include <freetype/fterrors.h>

namespace vull::ui {

Renderer::Renderer(const vk::Context &context, vk::RenderGraph &render_graph, const vk::Swapchain &swapchain,
                   vk::ImageResource &swapchain_resource, vkb::ShaderModule vertex_shader,
                   vkb::ShaderModule fragment_shader)
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

    // TODO: Dynamic resizing.
    vkb::BufferCreateInfo ui_data_buffer_ci{
        .sType = vkb::StructureType::BufferCreateInfo,
        .size = sizeof(Vec2f) + sizeof(Object) * 4096,
        .usage = vkb::BufferUsage::StorageBuffer,
        .sharingMode = vkb::SharingMode::Exclusive,
    };
    VULL_ENSURE(context.vkCreateBuffer(&ui_data_buffer_ci, &m_ui_data_buffer) == vkb::Result::Success);

    vkb::MemoryRequirements ui_data_buffer_requirements{};
    context.vkGetBufferMemoryRequirements(m_ui_data_buffer, &ui_data_buffer_requirements);
    m_ui_data_buffer_memory = context.allocate_memory(ui_data_buffer_requirements, vk::MemoryType::HostVisible);
    VULL_ENSURE(context.vkBindBufferMemory(m_ui_data_buffer, m_ui_data_buffer_memory, 0) == vkb::Result::Success);

    void *ui_data = nullptr;
    context.vkMapMemory(m_ui_data_buffer_memory, 0, vkb::k_whole_size, 0, &ui_data);
    m_scaling_ratio = reinterpret_cast<Vec2f *>(ui_data);
    m_objects = reinterpret_cast<Object *>(m_scaling_ratio + 1);

    Array descriptor_pool_sizes{
        vkb::DescriptorPoolSize{
            .type = vkb::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
        },
        vkb::DescriptorPoolSize{
            // TODO: Dynamic count or glyph streaming.
            .type = vkb::DescriptorType::CombinedImageSampler,
            .descriptorCount = 2000,
        },
    };
    vkb::DescriptorPoolCreateInfo descriptor_pool_ci{
        .sType = vkb::StructureType::DescriptorPoolCreateInfo,
        .maxSets = 1,
        .poolSizeCount = descriptor_pool_sizes.size(),
        .pPoolSizes = descriptor_pool_sizes.data(),
    };
    VULL_ENSURE(context.vkCreateDescriptorPool(&descriptor_pool_ci, &m_descriptor_pool) == vkb::Result::Success);

    Array set_bindings{
        vkb::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Vertex | vkb::ShaderStage::Fragment,
        },
        vkb::DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = vkb::DescriptorType::CombinedImageSampler,
            .descriptorCount = descriptor_pool_sizes[1].descriptorCount,
            .stageFlags = vkb::ShaderStage::Fragment,
        },
    };
    Array set_binding_flags{
        vkb::DescriptorBindingFlags::None,
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
        .bindingCount = set_bindings.size(),
        .pBindings = set_bindings.data(),
    };
    VULL_ENSURE(context.vkCreateDescriptorSetLayout(&set_layout_ci, &m_descriptor_set_layout) == vkb::Result::Success);

    vkb::DescriptorSetAllocateInfo descriptor_set_ai{
        .sType = vkb::StructureType::DescriptorSetAllocateInfo,
        .descriptorPool = m_descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_descriptor_set_layout,
    };
    VULL_ENSURE(context.vkAllocateDescriptorSets(&descriptor_set_ai, &m_descriptor_set) == vkb::Result::Success);

    vkb::PipelineLayoutCreateInfo pipeline_layout_ci{
        .sType = vkb::StructureType::PipelineLayoutCreateInfo,
        .setLayoutCount = 1,
        .pSetLayouts = &m_descriptor_set_layout,
    };
    VULL_ENSURE(context.vkCreatePipelineLayout(&pipeline_layout_ci, &m_pipeline_layout) == vkb::Result::Success);

    vkb::PipelineVertexInputStateCreateInfo vertex_input_state{
        .sType = vkb::StructureType::PipelineVertexInputStateCreateInfo,
    };
    vkb::PipelineInputAssemblyStateCreateInfo input_assembly_state{
        .sType = vkb::StructureType::PipelineInputAssemblyStateCreateInfo,
        .topology = vkb::PrimitiveTopology::TriangleList,
    };

    vkb::Rect2D scissor{
        .extent = swapchain.extent_2D(),
    };
    vkb::Viewport viewport{
        .width = static_cast<float>(swapchain.extent_2D().width),
        .height = static_cast<float>(swapchain.extent_2D().height),
        .maxDepth = 1.0f,
    };
    vkb::PipelineViewportStateCreateInfo viewport_state{
        .sType = vkb::StructureType::PipelineViewportStateCreateInfo,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    vkb::PipelineRasterizationStateCreateInfo rasterisation_state{
        .sType = vkb::StructureType::PipelineRasterizationStateCreateInfo,
        .polygonMode = vkb::PolygonMode::Fill,
        .cullMode = vkb::CullMode::None,
        .frontFace = vkb::FrontFace::Clockwise,
        .lineWidth = 1.0f,
    };

    vkb::PipelineMultisampleStateCreateInfo multisample_state{
        .sType = vkb::StructureType::PipelineMultisampleStateCreateInfo,
        .rasterizationSamples = vkb::SampleCount::_1,
        .minSampleShading = 1.0f,
    };

    vkb::PipelineColorBlendAttachmentState blend_attachment{
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
    vkb::PipelineColorBlendStateCreateInfo blend_state{
        .sType = vkb::StructureType::PipelineColorBlendStateCreateInfo,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment,
    };

    Array shader_stage_cis{
        vkb::PipelineShaderStageCreateInfo{
            .sType = vkb::StructureType::PipelineShaderStageCreateInfo,
            .stage = vkb::ShaderStage::Vertex,
            .module = vertex_shader,
            .pName = "main",
        },
        vkb::PipelineShaderStageCreateInfo{
            .sType = vkb::StructureType::PipelineShaderStageCreateInfo,
            .stage = vkb::ShaderStage::Fragment,
            .module = fragment_shader,
            .pName = "main",
        },
    };
    const auto colour_format = vkb::Format::B8G8R8A8Unorm;
    vkb::PipelineRenderingCreateInfo rendering_create_info{
        .sType = vkb::StructureType::PipelineRenderingCreateInfo,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colour_format,
    };
    vkb::GraphicsPipelineCreateInfo pipeline_ci{
        .sType = vkb::StructureType::GraphicsPipelineCreateInfo,
        .pNext = &rendering_create_info,
        .stageCount = shader_stage_cis.size(),
        .pStages = shader_stage_cis.data(),
        .pVertexInputState = &vertex_input_state,
        .pInputAssemblyState = &input_assembly_state,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterisation_state,
        .pMultisampleState = &multisample_state,
        .pColorBlendState = &blend_state,
        .layout = m_pipeline_layout,
    };
    VULL_ENSURE(context.vkCreateGraphicsPipelines(nullptr, 1, &pipeline_ci, &m_pipeline) == vkb::Result::Success);

    vkb::DescriptorBufferInfo ui_data_buffer_info{
        .buffer = m_ui_data_buffer,
        .range = vkb::k_whole_size,
    };
    vkb::WriteDescriptorSet ui_data_buffer_descriptor_write{
        .sType = vkb::StructureType::WriteDescriptorSet,
        .dstSet = m_descriptor_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = vkb::DescriptorType::StorageBuffer,
        .pBufferInfo = &ui_data_buffer_info,
    };
    context.vkUpdateDescriptorSets(1, &ui_data_buffer_descriptor_write, 0, nullptr);

    auto &ui_data_resource = render_graph.add_storage_buffer("UI data");
    ui_data_resource.set_buffer(m_ui_data_buffer);
    auto &ui_pass = render_graph.add_graphics_pass("UI pass");
    ui_pass.reads_from(ui_data_resource);
    ui_pass.writes_to(swapchain_resource);
    ui_pass.set_on_record([this, &swapchain_resource](const vk::CommandBuffer &cmd_buf) {
        *m_scaling_ratio = Vec2f(m_global_scale) / m_swapchain.dimensions();
        cmd_buf.bind_descriptor_sets(vkb::PipelineBindPoint::Graphics, m_pipeline_layout, m_descriptor_set);
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
        cmd_buf.bind_pipeline(vkb::PipelineBindPoint::Graphics, m_pipeline);
        cmd_buf.begin_rendering(rendering_info);
        cmd_buf.draw(6, exchange(m_object_index, 0u));
        cmd_buf.end_rendering();
    });
}

Renderer::~Renderer() {
    m_context.vkDestroyPipeline(m_pipeline);
    m_context.vkDestroyPipelineLayout(m_pipeline_layout);
    m_context.vkDestroyDescriptorSetLayout(m_descriptor_set_layout);
    m_context.vkDestroyDescriptorPool(m_descriptor_pool);
    m_context.vkFreeMemory(m_ui_data_buffer_memory);
    m_context.vkDestroyBuffer(m_ui_data_buffer);
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
    m_objects[m_object_index++] = {
        .colour = colour,
        .position = position,
        .scale = scale,
        .type = ObjectType::Rect,
    };
}

void Renderer::draw_text(GpuFont &font, const Vec3f &colour, const Vec2f &position, StringView text) {
    float cursor_x = position.x();
    float cursor_y = position.y();
    for (auto [glyph_index, x_advance, y_advance, x_offset, y_offset] : font.shape(text)) {
        const auto &glyph = font.cached_glyph(glyph_index);
        if (!glyph) {
            font.rasterise(glyph_index, m_descriptor_set, m_font_sampler);
        }
        Vec2f glyph_position(cursor_x + static_cast<float>(x_offset) / 64.0f,
                             cursor_y + static_cast<float>(y_offset) / 64.0f);
        glyph_position += {glyph->disp_x, glyph->disp_y};
        m_objects[m_object_index++] = {
            .colour = {colour.x(), colour.y(), colour.z(), 1.0f},
            .position = glyph_position,
            .scale = Vec2f(64.0f),
            .glyph_index = glyph_index,
            .type = ObjectType::TextGlyph,
        };
        cursor_x += static_cast<float>(x_advance) / 64.0f;
        cursor_y += static_cast<float>(y_advance) / 64.0f;
    }
}

} // namespace vull::ui
