#include <vull/rendering/GraphicsStage.hh>

#include <vull/rendering/RenderBuffer.hh>
#include <vull/rendering/RenderIndexBuffer.hh>
#include <vull/rendering/RenderTexture.hh>
#include <vull/rendering/RenderVertexBuffer.hh>
#include <vull/support/Assert.hh>
#include <vull/vulkan/Device.hh>
#include <vull/vulkan/Shader.hh>

namespace {

VkIndexType vk_index_type(IndexType index_type) {
    switch (index_type) {
    case IndexType::UInt16:
        return VK_INDEX_TYPE_UINT16;
    case IndexType::UInt32:
        return VK_INDEX_TYPE_UINT32;
    default:
        ENSURE_NOT_REACHED();
    }
}

} // namespace

GraphicsStage::~GraphicsStage() {
    // TODO: Maybe put this in RenderStage destructor since both compute and gfx have same VkPipeline.
    vkDestroyPipeline(**m_device, m_pipeline, nullptr);
    vkDestroyRenderPass(**m_device, m_render_pass, nullptr);
    vkDestroyFramebuffer(**m_device, m_framebuffer, nullptr);
}

void GraphicsStage::add_input(RenderTexture *texture) {
    m_inputs.push(texture);
    // TODO
    // texture->m_readers.push(this);
}

void GraphicsStage::add_output(RenderTexture *texture) {
    m_outputs.push(texture);
    texture->m_writers.push(this);
}

void GraphicsStage::build_objects(const Device &device, ExecutableGraph *executable_graph) {
    RenderStage::build_objects(device, executable_graph);

    Vector<VkAttachmentDescription> attachments;
    Vector<VkAttachmentReference> attachment_refs;
    VkSubpassDescription subpass{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    };
    VkAttachmentReference depth_reference{
        .attachment = 0,
    };

    for (const auto *input : m_inputs) {
        m_texture_order.push(input);
        if (input->type() == TextureType::Depth) {
            depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            subpass.pDepthStencilAttachment = &depth_reference;
        } else {
            attachment_refs.push(VkAttachmentReference{
                .attachment = attachments.size(),
                .layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR,
            });
        }
        attachments.push(VkAttachmentDescription{
            .format = input->format(),
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = m_initial_layouts[input],
            .finalLayout = m_final_layouts[input],
        });
    }
    for (const auto *output : m_outputs) {
        m_texture_order.push(output);
        if (output->type() == TextureType::Depth) {
            depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            subpass.pDepthStencilAttachment = &depth_reference;
        } else {
            attachment_refs.push(VkAttachmentReference{
                .attachment = attachments.size(),
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            });
        }
        attachments.push(VkAttachmentDescription{
            .format = output->format(),
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = m_initial_layouts[output],
            .finalLayout = m_final_layouts[output],
        });
    }

    subpass.colorAttachmentCount = attachment_refs.size();
    subpass.pColorAttachments = attachment_refs.data();

    VkSubpassDependency subpass_dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    };
    VkRenderPassCreateInfo render_pass_ci{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = attachments.size(),
        .pAttachments = attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &subpass_dependency,
    };
    ENSURE(vkCreateRenderPass(*device, &render_pass_ci, nullptr, &m_render_pass) == VK_SUCCESS);

    Vector<VkFramebufferAttachmentImageInfo> attachment_infos;
    Vector<VkFormat> view_formats;
    attachment_infos.ensure_capacity(m_texture_order.size());
    view_formats.ensure_capacity(m_texture_order.size());
    for (const auto *texture : m_texture_order) {
        VkImageUsageFlags usage = 0;
        switch (texture->type()) {
        case TextureType::Depth:
            usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            break;
        default:
            usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            break;
        }
        if (!texture->readers().empty()) {
            usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        }
        attachment_infos.push(VkFramebufferAttachmentImageInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,
            .usage = usage,
            .width = texture->extent().width,
            .height = texture->extent().height,
            .layerCount = 1,
            .viewFormatCount = 1,
            .pViewFormats = &view_formats.emplace(texture->format()),
        });
    }
    VkFramebufferAttachmentsCreateInfo attachments_ci{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO,
        .attachmentImageInfoCount = attachment_infos.size(),
        .pAttachmentImageInfos = attachment_infos.data(),
    };
    VkFramebufferCreateInfo framebuffer_ci{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = &attachments_ci,
        .flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT,
        .renderPass = m_render_pass,
        .attachmentCount = attachment_infos.size(),
        .width = m_outputs[0]->extent().width,
        .height = m_outputs[0]->extent().height,
        .layers = 1,
    };
    ENSURE(vkCreateFramebuffer(*device, &framebuffer_ci, nullptr, &m_framebuffer) == VK_SUCCESS);

    // Build vertex layout attributes and bindings.
    Vector<VkVertexInputAttributeDescription> vertex_attributes;
    Vector<VkVertexInputBindingDescription> vertex_bindings;
    for (const auto *resource : m_reads) {
        const auto *vertex_buffer = resource->as<RenderVertexBuffer>();
        if (vertex_buffer == nullptr) {
            continue;
        }

        // Binding is the order in which reads_from(vertex_buffer) is called.
        const std::uint32_t binding = vertex_bindings.size();
        for (auto attribute : vertex_buffer->vertex_attributes()) {
            attribute.binding = binding;
            vertex_attributes.push(attribute);
        }
        vertex_bindings.push(VkVertexInputBindingDescription{
            .binding = binding,
            .stride = vertex_buffer->element_size(),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        });
    }
    VkPipelineVertexInputStateCreateInfo vertex_input{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = vertex_bindings.size(),
        .pVertexBindingDescriptions = vertex_bindings.data(),
        .vertexAttributeDescriptionCount = vertex_attributes.size(),
        .pVertexAttributeDescriptions = vertex_attributes.data(),
    };
    VkPipelineInputAssemblyStateCreateInfo input_assembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkRect2D scissor{
        .extent{m_outputs[0]->extent().width, m_outputs[0]->extent().height},
    };
    VkViewport viewport{
        .width = static_cast<float>(scissor.extent.width),
        .height = static_cast<float>(scissor.extent.height),
        .maxDepth = 1.0f,
    };
    VkPipelineViewportStateCreateInfo viewport_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    VkPipelineRasterizationStateCreateInfo rasterisation_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisample_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .minSampleShading = 1.0f,
    };

    const bool has_depth_stencil_input =
        std::find_if(m_inputs.begin(), m_inputs.end(), [](const RenderTexture *texture) {
            return texture->type() == TextureType::Depth;
        }) != m_inputs.end();
    const bool has_depth_stencil_output =
        std::find_if(m_outputs.begin(), m_outputs.end(), [](const RenderTexture *texture) {
            return texture->type() == TextureType::Depth;
        }) != m_outputs.end();
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = static_cast<VkBool32>(has_depth_stencil_input || has_depth_stencil_output),
        .depthWriteEnable = static_cast<VkBool32>(has_depth_stencil_output),
        .depthCompareOp = has_depth_stencil_output ? VK_COMPARE_OP_LESS : VK_COMPARE_OP_EQUAL,
    };

    VkPipelineColorBlendAttachmentState blend_attachment{
        // NOLINTNEXTLINE
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                          VK_COLOR_COMPONENT_A_BIT, // NOLINT
    };
    VkPipelineColorBlendStateCreateInfo blend_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment,
    };

    Vector<VkPipelineShaderStageCreateInfo> shader_stage_cis;
    shader_stage_cis.ensure_capacity(m_shaders.size());
    for (const auto *shader : m_shaders) {
        shader_stage_cis.push(VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = shader->stage(),
            .module = **shader,
            .pName = "main",
            .pSpecializationInfo = &m_specialisation_info,
        });
    }

    VkGraphicsPipelineCreateInfo pipeline_ci{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = shader_stage_cis.size(),
        .pStages = shader_stage_cis.data(),
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterisation_state,
        .pMultisampleState = &multisample_state,
        .pDepthStencilState = &depth_stencil_state,
        .pColorBlendState = &blend_state,
        .layout = m_pipeline_layout,
        .renderPass = m_render_pass,
    };
    ENSURE(vkCreateGraphicsPipelines(*device, nullptr, 1, &pipeline_ci, nullptr, &m_pipeline) == VK_SUCCESS);
}

void GraphicsStage::start_recording(VkCommandBuffer cmd_buf) const {
    RenderStage::start_recording(cmd_buf);
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    // TODO: Can cache clear_values and attachments.

    Vector<VkClearValue> clear_values;
    clear_values.ensure_capacity(m_texture_order.size());
    for (const auto *texture : m_texture_order) {
        clear_values.push(texture->clear_value());
    }

    Vector<VkImageView> attachments;
    attachments.ensure_capacity(m_texture_order.size());
    for (const auto *texture : m_texture_order) {
        auto *image_view = texture->image_view();
        ASSERT(image_view != nullptr);
        attachments.push(image_view);
    }
    VkRenderPassAttachmentBeginInfo attachment_bi{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,
        .attachmentCount = attachments.size(),
        .pAttachments = attachments.data(),
    };
    VkRenderPassBeginInfo render_pass_bi{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = &attachment_bi,
        .renderPass = m_render_pass,
        .framebuffer = m_framebuffer,
        .renderArea{.extent{m_outputs[0]->extent().width, m_outputs[0]->extent().height}},
        .clearValueCount = clear_values.size(),
        .pClearValues = clear_values.data(),
    };
    vkCmdBeginRenderPass(cmd_buf, &render_pass_bi, VK_SUBPASS_CONTENTS_INLINE);

    // Bind index and vertex buffers.
    // TODO: Use small stack storage Vector when available to reduce heap allocations per-frame.
    Vector<VkBuffer> vertex_buffers;
    Vector<VkDeviceSize> vertex_offsets;
    for (const auto *resource : m_reads) {
        if (const auto *index_buffer = resource->as<RenderIndexBuffer>()) {
            vkCmdBindIndexBuffer(cmd_buf, index_buffer->buffer(), 0, vk_index_type(index_buffer->index_type()));
        } else if (const auto *vertex_buffer = resource->as<RenderVertexBuffer>()) {
            vertex_buffers.push(vertex_buffer->buffer());
            vertex_offsets.push(0);
        }
    }
    vkCmdBindVertexBuffers(cmd_buf, 0, vertex_buffers.size(), vertex_buffers.data(), vertex_offsets.data());
}

VkCommandBuffer GraphicsStage::stop_recording() const {
    vkCmdEndRenderPass(m_cmd_buf);
    return RenderStage::stop_recording();
}

void GraphicsStage::draw_indexed(std::uint32_t index_count, std::uint32_t first_index) {
    vkCmdDrawIndexed(m_cmd_buf, index_count, 1, first_index, 0, 0);
}
