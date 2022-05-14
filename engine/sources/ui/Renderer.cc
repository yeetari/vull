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
#include <vull/vulkan/Swapchain.hh>
#include <vull/vulkan/Vulkan.hh>

#include <ft2build.h> // IWYU pragma: keep
// IWYU pragma: no_include "freetype/config/ftheader.h"

#include <freetype/freetype.h>
#include <freetype/fterrors.h>

namespace vull::ui {

Renderer::Renderer(const VkContext &context, const Swapchain &swapchain, vk::ShaderModule vertex_shader,
                   vk::ShaderModule fragment_shader)
    : m_context(context), m_swapchain(swapchain) {
    VULL_ENSURE(FT_Init_FreeType(&m_ft_library) == FT_Err_Ok);

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

    // TODO: Dynamic resizing.
    vk::BufferCreateInfo ui_data_buffer_ci{
        .sType = vk::StructureType::BufferCreateInfo,
        .size = sizeof(Vec2f) + sizeof(Object) * 2000,
        .usage = vk::BufferUsage::StorageBuffer,
        .sharingMode = vk::SharingMode::Exclusive,
    };
    VULL_ENSURE(context.vkCreateBuffer(&ui_data_buffer_ci, &m_ui_data_buffer) == vk::Result::Success);

    vk::MemoryRequirements ui_data_buffer_requirements{};
    context.vkGetBufferMemoryRequirements(m_ui_data_buffer, &ui_data_buffer_requirements);
    m_ui_data_buffer_memory = context.allocate_memory(ui_data_buffer_requirements, MemoryType::HostVisible);
    VULL_ENSURE(context.vkBindBufferMemory(m_ui_data_buffer, m_ui_data_buffer_memory, 0) == vk::Result::Success);

    void *ui_data = nullptr;
    context.vkMapMemory(m_ui_data_buffer_memory, 0, vk::k_whole_size, 0, &ui_data);
    m_scaling_ratio = reinterpret_cast<Vec2f *>(ui_data);
    m_objects = reinterpret_cast<Object *>(m_scaling_ratio + 1);

    Array descriptor_pool_sizes{
        vk::DescriptorPoolSize{
            .type = vk::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
        },
        vk::DescriptorPoolSize{
            // TODO: Dynamic count or glyph streaming.
            .type = vk::DescriptorType::CombinedImageSampler,
            .descriptorCount = 2000,
        },
    };
    vk::DescriptorPoolCreateInfo descriptor_pool_ci{
        .sType = vk::StructureType::DescriptorPoolCreateInfo,
        .maxSets = 1,
        .poolSizeCount = descriptor_pool_sizes.size(),
        .pPoolSizes = descriptor_pool_sizes.data(),
    };
    VULL_ENSURE(context.vkCreateDescriptorPool(&descriptor_pool_ci, &m_descriptor_pool) == vk::Result::Success);

    Array set_bindings{
        vk::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStage::Vertex | vk::ShaderStage::Fragment,
        },
        vk::DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = vk::DescriptorType::CombinedImageSampler,
            .descriptorCount = descriptor_pool_sizes[1].descriptorCount,
            .stageFlags = vk::ShaderStage::Fragment,
        },
    };
    Array set_binding_flags{
        vk::DescriptorBindingFlags::None,
        vk::DescriptorBindingFlags::PartiallyBound,
    };
    vk::DescriptorSetLayoutBindingFlagsCreateInfo set_binding_flags_ci{
        .sType = vk::StructureType::DescriptorSetLayoutBindingFlagsCreateInfo,
        .bindingCount = set_bindings.size(),
        .pBindingFlags = set_binding_flags.data(),
    };
    vk::DescriptorSetLayoutCreateInfo set_layout_ci{
        .sType = vk::StructureType::DescriptorSetLayoutCreateInfo,
        .pNext = &set_binding_flags_ci,
        .bindingCount = set_bindings.size(),
        .pBindings = set_bindings.data(),
    };
    VULL_ENSURE(context.vkCreateDescriptorSetLayout(&set_layout_ci, &m_descriptor_set_layout) == vk::Result::Success);

    vk::DescriptorSetAllocateInfo descriptor_set_ai{
        .sType = vk::StructureType::DescriptorSetAllocateInfo,
        .descriptorPool = m_descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_descriptor_set_layout,
    };
    VULL_ENSURE(context.vkAllocateDescriptorSets(&descriptor_set_ai, &m_descriptor_set) == vk::Result::Success);

    vk::PipelineLayoutCreateInfo pipeline_layout_ci{
        .sType = vk::StructureType::PipelineLayoutCreateInfo,
        .setLayoutCount = 1,
        .pSetLayouts = &m_descriptor_set_layout,
    };
    VULL_ENSURE(context.vkCreatePipelineLayout(&pipeline_layout_ci, &m_pipeline_layout) == vk::Result::Success);

    vk::PipelineVertexInputStateCreateInfo vertex_input_state{
        .sType = vk::StructureType::PipelineVertexInputStateCreateInfo,
    };
    vk::PipelineInputAssemblyStateCreateInfo input_assembly_state{
        .sType = vk::StructureType::PipelineInputAssemblyStateCreateInfo,
        .topology = vk::PrimitiveTopology::TriangleList,
    };

    vk::Rect2D scissor{
        .extent = swapchain.extent_2D(),
    };
    vk::Viewport viewport{
        .width = static_cast<float>(swapchain.extent_2D().width),
        .height = static_cast<float>(swapchain.extent_2D().height),
        .maxDepth = 1.0f,
    };
    vk::PipelineViewportStateCreateInfo viewport_state{
        .sType = vk::StructureType::PipelineViewportStateCreateInfo,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    vk::PipelineRasterizationStateCreateInfo rasterisation_state{
        .sType = vk::StructureType::PipelineRasterizationStateCreateInfo,
        .polygonMode = vk::PolygonMode::Fill,
        .cullMode = vk::CullMode::None,
        .frontFace = vk::FrontFace::Clockwise,
        .lineWidth = 1.0f,
    };

    vk::PipelineMultisampleStateCreateInfo multisample_state{
        .sType = vk::StructureType::PipelineMultisampleStateCreateInfo,
        .rasterizationSamples = vk::SampleCount::_1,
        .minSampleShading = 1.0f,
    };

    vk::PipelineColorBlendAttachmentState blend_attachment{
        .blendEnable = true,
        .srcColorBlendFactor = vk::BlendFactor::SrcAlpha,
        .dstColorBlendFactor = vk::BlendFactor::OneMinusSrcAlpha,
        .colorBlendOp = vk::BlendOp::Add,
        .srcAlphaBlendFactor = vk::BlendFactor::One,
        .dstAlphaBlendFactor = vk::BlendFactor::Zero,
        .alphaBlendOp = vk::BlendOp::Add,
        .colorWriteMask = vk::ColorComponent::R | vk::ColorComponent::G | vk::ColorComponent::B | vk::ColorComponent::A,
    };
    vk::PipelineColorBlendStateCreateInfo blend_state{
        .sType = vk::StructureType::PipelineColorBlendStateCreateInfo,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment,
    };

    Array shader_stage_cis{
        vk::PipelineShaderStageCreateInfo{
            .sType = vk::StructureType::PipelineShaderStageCreateInfo,
            .stage = vk::ShaderStage::Vertex,
            .module = vertex_shader,
            .pName = "main",
        },
        vk::PipelineShaderStageCreateInfo{
            .sType = vk::StructureType::PipelineShaderStageCreateInfo,
            .stage = vk::ShaderStage::Fragment,
            .module = fragment_shader,
            .pName = "main",
        },
    };
    const auto colour_format = vk::Format::B8G8R8A8Unorm;
    vk::PipelineRenderingCreateInfo rendering_create_info{
        .sType = vk::StructureType::PipelineRenderingCreateInfo,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colour_format,
    };
    vk::GraphicsPipelineCreateInfo pipeline_ci{
        .sType = vk::StructureType::GraphicsPipelineCreateInfo,
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
    VULL_ENSURE(context.vkCreateGraphicsPipelines(nullptr, 1, &pipeline_ci, &m_pipeline) == vk::Result::Success);

    vk::DescriptorBufferInfo ui_data_buffer_info{
        .buffer = m_ui_data_buffer,
        .range = vk::k_whole_size,
    };
    vk::WriteDescriptorSet ui_data_buffer_descriptor_write{
        .sType = vk::StructureType::WriteDescriptorSet,
        .dstSet = m_descriptor_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::StorageBuffer,
        .pBufferInfo = &ui_data_buffer_info,
    };
    context.vkUpdateDescriptorSets(1, &ui_data_buffer_descriptor_write, 0, nullptr);
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

void Renderer::render(const CommandBuffer &cmd_buf, uint32_t image_index) {
    *m_scaling_ratio = Vec2f(m_global_scale) / m_swapchain.dimensions();
    cmd_buf.bind_descriptor_sets(vk::PipelineBindPoint::Graphics, m_pipeline_layout, {&m_descriptor_set, 1});
    vk::RenderingAttachmentInfo colour_write_attachment{
        .sType = vk::StructureType::RenderingAttachmentInfo,
        .imageView = m_swapchain.image_view(image_index),
        .imageLayout = vk::ImageLayout::ColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::Load,
        .storeOp = vk::AttachmentStoreOp::Store,
    };
    vk::RenderingInfo rendering_info{
        .sType = vk::StructureType::RenderingInfo,
        .renderArea{
            .extent = m_swapchain.extent_2D(),
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colour_write_attachment,
    };
    cmd_buf.begin_rendering(rendering_info);
    cmd_buf.bind_pipeline(vk::PipelineBindPoint::Graphics, m_pipeline);
    cmd_buf.draw(6, exchange(m_object_index, 0u));
    cmd_buf.end_rendering();
}

} // namespace vull::ui
