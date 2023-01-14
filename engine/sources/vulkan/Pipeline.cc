#include <vull/vulkan/Pipeline.hh>
#include <vull/vulkan/PipelineBuilder.hh>

#include <vull/support/Assert.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Shader.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull::vk {

Pipeline::Pipeline(Pipeline &&other) : m_bind_point(other.m_bind_point) {
    m_context = vull::exchange(other.m_context, nullptr);
    m_pipeline = vull::exchange(other.m_pipeline, nullptr);
    m_layout = vull::exchange(other.m_layout, nullptr);
}

Pipeline::~Pipeline() {
    if (m_context != nullptr) {
        m_context->vkDestroyPipelineLayout(m_layout);
        m_context->vkDestroyPipeline(m_pipeline);
    }
}

Pipeline &Pipeline::operator=(Pipeline &&other) {
    Pipeline moved(vull::move(other));
    vull::swap(m_context, moved.m_context);
    vull::swap(m_pipeline, moved.m_pipeline);
    vull::swap(m_layout, moved.m_layout);
    vull::swap(m_bind_point, moved.m_bind_point);
    return *this;
}

PipelineBuilder &
PipelineBuilder::add_colour_attachment(vkb::Format format,
                                       Optional<const vkb::PipelineColorBlendAttachmentState &> blend_state) {
    vkb::PipelineColorBlendAttachmentState default_blend_state{
        .colorWriteMask =
            vkb::ColorComponent::R | vkb::ColorComponent::G | vkb::ColorComponent::B | vkb::ColorComponent::A,
    };
    // TODO: Optional::value_or()
    m_blend_states.push(blend_state ? *blend_state : default_blend_state);
    m_colour_formats.push(format);
    return *this;
}

PipelineBuilder &PipelineBuilder::add_set_layout(vkb::DescriptorSetLayout set_layout) {
    m_set_layouts.push(set_layout);
    return *this;
}

PipelineBuilder &PipelineBuilder::add_shader(const Shader &shader, Optional<const vkb::SpecializationInfo &> si) {
    m_shaders.push(shader);
    m_shader_cis.push({
        .sType = vkb::StructureType::PipelineShaderStageCreateInfo,
        .stage = shader.stage(),
        .module = shader.module(),
        .pName = "main",
        .pSpecializationInfo = si ? &*si : nullptr,
    });
    return *this;
}

PipelineBuilder &PipelineBuilder::set_cull_mode(vkb::CullMode cull_mode, vkb::FrontFace front_face) {
    m_cull_mode = cull_mode;
    m_front_face = front_face;
    return *this;
}

PipelineBuilder &PipelineBuilder::set_depth_bias(float cf, float sf) {
    m_depth_bias_cf = cf;
    m_depth_bias_sf = sf;
    return *this;
}

PipelineBuilder &PipelineBuilder::set_depth_format(vkb::Format format) {
    m_depth_format = format;
    return *this;
}

PipelineBuilder &PipelineBuilder::set_depth_params(vkb::CompareOp op, bool test_enabled, bool write_enabled) {
    m_depth_op = op;
    m_depth_test_enabled = test_enabled;
    m_depth_write_enabled = write_enabled;
    return *this;
}

PipelineBuilder &PipelineBuilder::set_polygon_mode(vkb::PolygonMode polygon_mode) {
    m_polygon_mode = polygon_mode;
    return *this;
}

PipelineBuilder &PipelineBuilder::set_push_constant_range(const vkb::PushConstantRange &push_constant_range) {
    m_push_constant_range = push_constant_range;
    return *this;
}

PipelineBuilder &PipelineBuilder::set_topology(vkb::PrimitiveTopology topology) {
    m_topology = topology;
    return *this;
}

PipelineBuilder &PipelineBuilder::set_viewport(vkb::Extent2D extent) {
    m_viewport_extent = extent;
    return *this;
}

PipelineBuilder &PipelineBuilder::set_viewport(vkb::Extent3D extent) {
    m_viewport_extent = {extent.width, extent.height};
    return *this;
}

Pipeline PipelineBuilder::build(const Context &context) {
    vkb::PipelineLayoutCreateInfo layout_ci{
        .sType = vkb::StructureType::PipelineLayoutCreateInfo,
        .setLayoutCount = m_set_layouts.size(),
        .pSetLayouts = m_set_layouts.data(),
        .pushConstantRangeCount = m_push_constant_range.size != 0 ? 1u : 0u,
        .pPushConstantRanges = &m_push_constant_range,
    };
    vkb::PipelineLayout layout;
    VULL_ENSURE(context.vkCreatePipelineLayout(&layout_ci, &layout) == vkb::Result::Success);

    if (m_colour_formats.empty() && m_depth_format == vkb::Format::Undefined) {
        VULL_ASSERT(m_shader_cis.size() == 1);
        vkb::ComputePipelineCreateInfo pipeline_ci{
            .sType = vkb::StructureType::ComputePipelineCreateInfo,
            // TODO: Is it bad to always pass this? (do any drivers care?)
            .flags = vkb::PipelineCreateFlags::DescriptorBufferEXT,
            .stage = m_shader_cis.first(),
            .layout = layout,
        };
        vkb::Pipeline pipeline;
        VULL_ENSURE(context.vkCreateComputePipelines(nullptr, 1, &pipeline_ci, &pipeline) == vkb::Result::Success);
        return {context, pipeline, layout, vkb::PipelineBindPoint::Compute};
    }

    vkb::PipelineRenderingCreateInfo rendering_ci{
        .sType = vkb::StructureType::PipelineRenderingCreateInfo,
        .colorAttachmentCount = m_colour_formats.size(),
        .pColorAttachmentFormats = m_colour_formats.data(),
        .depthAttachmentFormat = m_depth_format,
    };

    const Shader *vertex_shader = nullptr;
    for (const Shader &shader : m_shaders) {
        if (shader.stage() == vkb::ShaderStage::Vertex) {
            vertex_shader = &shader;
        }
    }
    const auto &vertex_attributes = vertex_shader->vertex_attributes();
    vkb::VertexInputBindingDescription vertex_binding{
        .stride = vertex_shader->vertex_stride(),
        .inputRate = vkb::VertexInputRate::Vertex,
    };
    vkb::PipelineVertexInputStateCreateInfo vertex_input_state_ci{
        .sType = vkb::StructureType::PipelineVertexInputStateCreateInfo,
        .vertexBindingDescriptionCount = !vertex_attributes.empty() ? 1u : 0u,
        .pVertexBindingDescriptions = &vertex_binding,
        .vertexAttributeDescriptionCount = vertex_attributes.size(),
        .pVertexAttributeDescriptions = vertex_attributes.data(),
    };

    vkb::PipelineInputAssemblyStateCreateInfo input_assembly_state_ci{
        .sType = vkb::StructureType::PipelineInputAssemblyStateCreateInfo,
        .topology = m_topology,
    };

    vkb::Viewport viewport{
        .width = static_cast<float>(m_viewport_extent.width),
        .height = static_cast<float>(m_viewport_extent.height),
        .maxDepth = 1.0f,
    };
    vkb::Rect2D scissor{
        .extent = m_viewport_extent,
    };
    vkb::PipelineViewportStateCreateInfo viewport_state_ci{
        .sType = vkb::StructureType::PipelineViewportStateCreateInfo,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    vkb::PipelineRasterizationStateCreateInfo rasterization_state_ci{
        .sType = vkb::StructureType::PipelineRasterizationStateCreateInfo,
        .polygonMode = m_polygon_mode,
        .cullMode = m_cull_mode,
        .frontFace = m_front_face,
        .depthBiasEnable = m_depth_bias_cf != 0.0f || m_depth_bias_sf != 0.0f,
        .depthBiasConstantFactor = m_depth_bias_cf,
        .depthBiasSlopeFactor = m_depth_bias_sf,
        .lineWidth = 1.0f,
    };

    vkb::PipelineMultisampleStateCreateInfo multisample_state_ci{
        .sType = vkb::StructureType::PipelineMultisampleStateCreateInfo,
        .rasterizationSamples = vkb::SampleCount::_1,
    };

    vkb::PipelineDepthStencilStateCreateInfo depth_stencil_state_ci{
        .sType = vkb::StructureType::PipelineDepthStencilStateCreateInfo,
        .depthTestEnable = m_depth_test_enabled,
        .depthWriteEnable = m_depth_write_enabled,
        .depthCompareOp = m_depth_op,
    };

    vkb::PipelineColorBlendStateCreateInfo blend_state_ci{
        .sType = vkb::StructureType::PipelineColorBlendStateCreateInfo,
        .attachmentCount = m_blend_states.size(),
        .pAttachments = m_blend_states.data(),
    };

    vkb::GraphicsPipelineCreateInfo pipeline_ci{
        .sType = vkb::StructureType::GraphicsPipelineCreateInfo,
        .pNext = &rendering_ci,
        .flags = vkb::PipelineCreateFlags::DescriptorBufferEXT,
        .stageCount = m_shader_cis.size(),
        .pStages = m_shader_cis.data(),
        .pVertexInputState = &vertex_input_state_ci,
        .pInputAssemblyState = &input_assembly_state_ci,
        .pViewportState = &viewport_state_ci,
        .pRasterizationState = &rasterization_state_ci,
        .pMultisampleState = &multisample_state_ci,
        .pDepthStencilState = &depth_stencil_state_ci,
        .pColorBlendState = &blend_state_ci,
        .layout = layout,
    };
    vkb::Pipeline pipeline;
    VULL_ENSURE(context.vkCreateGraphicsPipelines(nullptr, 1, &pipeline_ci, &pipeline) == vkb::Result::Success);
    return {context, pipeline, layout, vkb::PipelineBindPoint::Graphics};
}

} // namespace vull::vk
