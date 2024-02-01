#include <vull/vulkan/pipeline.hh>
#include <vull/vulkan/pipeline_builder.hh>

#include <vull/container/array.hh>
#include <vull/container/hash_map.hh>
#include <vull/container/vector.hh>
#include <vull/support/assert.hh>
#include <vull/support/optional.hh>
#include <vull/support/span.hh>
#include <vull/support/string.hh>
#include <vull/support/utility.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/shader.hh>
#include <vull/vulkan/vulkan.hh>

#include <stddef.h>
#include <stdint.h>

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

PipelineBuilder &PipelineBuilder::add_shader(const Shader &shader) {
    m_shaders.push(shader);
    return *this;
}

template <>
PipelineBuilder &PipelineBuilder::set_constant(String name, bool value) {
    m_constants.set(vull::move(name), value ? 1 : 0);
    return *this;
}
template <>
PipelineBuilder &PipelineBuilder::set_constant(String name, uint32_t value) {
    m_constants.set(vull::move(name), value);
    return *this;
}
template <>
PipelineBuilder &PipelineBuilder::set_constant(String name, uint64_t value) {
    m_constants.set(vull::move(name), value);
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

PipelineResult PipelineBuilder::build(const Context &context) {
    vkb::PipelineLayoutCreateInfo layout_ci{
        .sType = vkb::StructureType::PipelineLayoutCreateInfo,
        .setLayoutCount = m_set_layouts.size(),
        .pSetLayouts = m_set_layouts.data(),
        .pushConstantRangeCount = m_push_constant_range.size != 0 ? 1u : 0u,
        .pPushConstantRanges = &m_push_constant_range,
    };
    vkb::PipelineLayout layout;
    if (auto result = context.vkCreatePipelineLayout(&layout_ci, &layout); result != vkb::Result::Success) {
        return result;
    }

    // TODO(small-vector)
    Vector<vkb::PipelineShaderStageCreateInfo> shader_cis;
    Vector<vkb::SpecializationInfo> specialization_infos;
    specialization_infos.ensure_capacity(m_shaders.size());
    for (const Shader &shader : m_shaders) {
        Vector<vkb::SpecializationMapEntry> specialization_map_entries;
        Vector<size_t> specialization_values;
        for (const auto &constant : shader.constants()) {
            if (!m_constants.contains(constant.name)) {
                return UnspecifiedConstantError{constant.name};
            }

            specialization_map_entries.push({
                .constantID = constant.id,
                .offset = specialization_values.size_bytes(),
                .size = constant.size,
            });
            specialization_values.push(*m_constants.get(constant.name));
        }

        auto &specialization_info = specialization_infos.emplace(vkb::SpecializationInfo{
            .mapEntryCount = specialization_map_entries.size(),
            .pMapEntries = specialization_map_entries.take_all().data(),
            .dataSize = specialization_values.size_bytes(),
            .pData = specialization_values.take_all().data(),
        });
        for (const auto &[name, stage, interface_ids] : shader.entry_points()) {
            shader_cis.push({
                .sType = vkb::StructureType::PipelineShaderStageCreateInfo,
                .stage = stage,
                .module = shader.module(),
                .pName = name.data(),
                .pSpecializationInfo = &specialization_info,
            });
        }
    }

    if (m_colour_formats.empty() && m_depth_format == vkb::Format::Undefined) {
        VULL_ASSERT(shader_cis.size() == 1);
        vkb::ComputePipelineCreateInfo pipeline_ci{
            .sType = vkb::StructureType::ComputePipelineCreateInfo,
            // TODO: Is it bad to always pass this? (do any drivers care?)
            .flags = vkb::PipelineCreateFlags::DescriptorBufferEXT,
            .stage = shader_cis.first(),
            .layout = layout,
        };
        vkb::Pipeline pipeline;
        if (auto result = context.vkCreateComputePipelines(nullptr, 1, &pipeline_ci, &pipeline);
            result != vkb::Result::Success) {
            return result;
        }
        return Pipeline(context, pipeline, layout, vkb::PipelineBindPoint::Compute);
    }

    vkb::PipelineRenderingCreateInfo rendering_ci{
        .sType = vkb::StructureType::PipelineRenderingCreateInfo,
        .colorAttachmentCount = m_colour_formats.size(),
        .pColorAttachmentFormats = m_colour_formats.data(),
        .depthAttachmentFormat = m_depth_format,
    };

    // TODO: Remove loop when there is only one Shader.
    const Shader *vertex_shader = nullptr;
    for (const Shader &shader : m_shaders) {
        for (const auto &entry_point : shader.entry_points()) {
            if (entry_point.stage == vkb::ShaderStage::Vertex) {
                vertex_shader = &shader;
            }
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

    vkb::PipelineViewportStateCreateInfo viewport_state_ci{
        .sType = vkb::StructureType::PipelineViewportStateCreateInfo,
        .viewportCount = 1,
        .scissorCount = 1,
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

    Array dynamic_states{
        vkb::DynamicState::Viewport,
        vkb::DynamicState::Scissor,
    };
    vkb::PipelineDynamicStateCreateInfo dynamic_state_ci{
        .sType = vkb::StructureType::PipelineDynamicStateCreateInfo,
        .dynamicStateCount = dynamic_states.size(),
        .pDynamicStates = dynamic_states.data(),
    };

    vkb::GraphicsPipelineCreateInfo pipeline_ci{
        .sType = vkb::StructureType::GraphicsPipelineCreateInfo,
        .pNext = &rendering_ci,
        .flags = vkb::PipelineCreateFlags::DescriptorBufferEXT,
        .stageCount = shader_cis.size(),
        .pStages = shader_cis.data(),
        .pVertexInputState = &vertex_input_state_ci,
        .pInputAssemblyState = &input_assembly_state_ci,
        .pViewportState = &viewport_state_ci,
        .pRasterizationState = &rasterization_state_ci,
        .pMultisampleState = &multisample_state_ci,
        .pDepthStencilState = &depth_stencil_state_ci,
        .pColorBlendState = &blend_state_ci,
        .pDynamicState = &dynamic_state_ci,
        .layout = layout,
    };
    vkb::Pipeline pipeline;
    if (auto result = context.vkCreateGraphicsPipelines(nullptr, 1, &pipeline_ci, &pipeline);
        result != vkb::Result::Success) {
        return result;
    }
    return Pipeline(context, pipeline, layout, vkb::PipelineBindPoint::Graphics);
}

} // namespace vull::vk
