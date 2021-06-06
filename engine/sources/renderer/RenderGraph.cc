#include <vull/renderer/RenderGraph.hh>

#include <vull/renderer/Device.hh>
#include <vull/renderer/Shader.hh>
#include <vull/renderer/Swapchain.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Box.hh>
#include <vull/support/Log.hh>
#include <vull/support/Vector.hh>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace {

VkBufferUsageFlags buffer_usage(BufferType type) {
    switch (type) {
    case BufferType::IndexBuffer:
        return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    case BufferType::StorageBuffer:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    case BufferType::UniformBuffer:
        return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    case BufferType::VertexBuffer:
        return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    default:
        ENSURE_NOT_REACHED();
    }
}

VkDescriptorType descriptor_type(BufferType type) {
    switch (type) {
    case BufferType::StorageBuffer:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case BufferType::UniformBuffer:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    default:
        ENSURE_NOT_REACHED();
    }
}

VkPipelineStageFlags barrier_stage(const RenderStage *stage, const RenderResource *resource) {
    if (stage->is<ComputeStage>()) {
        return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }

    // Else stage is a graphics stage.
    if (const auto *image = resource->as<ImageResource>()) {
        return image->type() == ImageType::Depth ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
                                                 : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    // TODO: We don't have enough info here to figure out which stage(s) requires the resource. Could we figure this out
    //       with reflection to find out which shaders access which resources?
    return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
}

} // namespace

void BufferResource::add_vertex_attribute(VkFormat format, std::uint32_t offset) {
    m_vertex_attributes.push(VkVertexInputAttributeDescription{
        .location = m_vertex_attributes.size(),
        .format = format,
        .offset = offset,
    });
}

void ComputeStage::set_shader(const Shader &shader, const VkSpecializationInfo *specialisation_info) {
    m_shader = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = *shader,
        .pName = "main",
        .pSpecializationInfo = specialisation_info,
    };
}

void GraphicsStage::set_vertex_shader(const Shader &shader, const VkSpecializationInfo *specialisation_info) {
    m_vertex_shader = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = *shader,
        .pName = "main",
        .pSpecializationInfo = specialisation_info,
    };
}

void GraphicsStage::set_fragment_shader(const Shader &shader, const VkSpecializationInfo *specialisation_info) {
    m_fragment_shader = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = *shader,
        .pName = "main",
        .pSpecializationInfo = specialisation_info,
    };
}

Box<CompiledGraph> RenderGraph::compile(const RenderResource *target) const {
    // TODO: Input validation, e.g. ensure graph is acyclic.
    Box<CompiledGraph> compiled_graph(new CompiledGraph(this));
    auto &barriers = compiled_graph->m_barriers;
    auto &semaphores = compiled_graph->m_semaphores;
    auto &stage_order = compiled_graph->m_stage_order;

    std::unordered_map<const RenderResource *, Vector<RenderStage *>> writers;
    for (const auto &stage : m_compute_stages) {
        for (const auto *resource : stage->m_writes) {
            writers[resource].push(*stage);
        }
    }
    for (const auto &stage : m_graphics_stages) {
        for (const auto *resource : stage->m_writes) {
            writers[resource].push(*stage);
        }
        for (const auto *resource : stage->m_outputs) {
            writers[resource].push(*stage);
        }
    }

    // Post order depth first search to build a linear order.
    std::unordered_set<RenderStage *> visited;
    std::function<void(RenderStage *)> dfs = [&](RenderStage *stage) {
        if (!visited.insert(stage).second) {
            return;
        }
        for (const auto *resource : stage->m_reads) {
            for (auto *writer : writers[resource]) {
                dfs(writer);
            }
        }
        stage_order.push(stage);
    };

    // Perform the DFS starting from the writers of the target.
    for (auto *writer : writers[target]) {
        dfs(writer);
    }

    // Insert barriers for resources.
    for (const auto *stage : stage_order) {
        for (const auto *resource : stage->m_reads) {
            for (auto *writer : writers[resource]) {
                if (std::find(stage_order.begin(), stage_order.end(), writer) >
                    std::find(stage_order.begin(), stage_order.end(), stage)) {
                    continue;
                }
                barriers.emplace(writer, stage, resource);
            }
        }
    }

    // Insert semaphores between stages that write to the same resource.
    for (const auto *resource : m_resources) {
        for (RenderStage *wait_stage = nullptr; auto *writer : writers[resource]) {
            if (wait_stage != nullptr) {
                semaphores.emplace(wait_stage, writer);
            }
            wait_stage = writer;
        }
    }
    return compiled_graph;
}

void CompiledGraph::build_compute_pipeline(const Device &device, const ComputeStage *stage,
                                           VkPipelineLayout pipeline_layout, VkPipeline *pipeline) {
    VkComputePipelineCreateInfo pipeline_ci{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stage->m_shader,
        .layout = pipeline_layout,
    };
    ENSURE(vkCreateComputePipelines(*device, nullptr, 1, &pipeline_ci, nullptr, pipeline) == VK_SUCCESS);
}

void CompiledGraph::build_graphics_pipeline(const Device &device, const GraphicsStage *stage,
                                            VkPipelineLayout pipeline_layout, VkRenderPass render_pass,
                                            VkPipeline *pipeline) {
    // Build vertex layout attributes and bindings.
    Vector<VkVertexInputAttributeDescription> vertex_attributes;
    Vector<VkVertexInputBindingDescription> vertex_bindings;
    for (const auto *resource : stage->m_reads) {
        const auto *buffer = resource->as<BufferResource>();
        if (buffer == nullptr || buffer->m_type != BufferType::VertexBuffer) {
            continue;
        }

        // For now, the binding is the order in which reads_from(vertex_buffer) is called, though this is quite fragile
        // and may change.
        const std::uint32_t binding = vertex_bindings.size();
        for (auto attribute : buffer->m_vertex_attributes) {
            attribute.binding = binding;
            vertex_attributes.push(attribute);
        }
        vertex_bindings.push(VkVertexInputBindingDescription{
            .binding = binding,
            .stride = buffer->m_element_size,
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
        .extent{stage->m_outputs[0]->m_extent.width, stage->m_outputs[0]->m_extent.height},
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
        std::find_if(stage->m_inputs.begin(), stage->m_inputs.end(), [](const ImageResource *image) {
            return image->m_type == ImageType::Depth;
        }) != stage->m_inputs.end();
    const bool has_depth_stencil_output =
        std::find_if(stage->m_outputs.begin(), stage->m_outputs.end(), [](const ImageResource *image) {
            return image->m_type == ImageType::Depth;
        }) != stage->m_outputs.end();
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

    Array<VkPipelineShaderStageCreateInfo, 2> shaders{};
    std::uint32_t shader_count = 0;
    if (stage->m_vertex_shader.module != nullptr) {
        shaders[shader_count++] = stage->m_vertex_shader;
    }
    if (stage->m_fragment_shader.module != nullptr) {
        shaders[shader_count++] = stage->m_fragment_shader;
    }

    VkGraphicsPipelineCreateInfo pipeline_ci{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = shader_count,
        .pStages = shaders.data(),
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterisation_state,
        .pMultisampleState = &multisample_state,
        .pDepthStencilState = &depth_stencil_state,
        .pColorBlendState = &blend_state,
        .layout = pipeline_layout,
        .renderPass = render_pass,
    };
    ENSURE(vkCreateGraphicsPipelines(*device, nullptr, 1, &pipeline_ci, nullptr, pipeline) == VK_SUCCESS);
}

// NOLINTNEXTLINE
Box<ExecutableGraph> CompiledGraph::build_objects(const Device &device, std::uint32_t frame_queue_length) {
    Box<ExecutableGraph> executable_graph(new ExecutableGraph(this, m_graph, device, frame_queue_length));
    auto &frame_datas = executable_graph->m_frame_datas;
    auto &image_orders = executable_graph->m_image_orders;
    auto &resource_bindings = executable_graph->m_resource_bindings;
    auto &descriptor_set_layouts = executable_graph->m_descriptor_set_layouts;
    auto &pipelines = executable_graph->m_pipelines;
    auto &pipeline_layouts = executable_graph->m_pipeline_layouts;
    auto &render_passes = executable_graph->m_render_passes;
    image_orders.resize(m_stage_order.size());
    resource_bindings.resize(m_stage_order.size());
    descriptor_set_layouts.resize(m_stage_order.size());
    pipelines.resize(m_stage_order.size());
    pipeline_layouts.resize(m_stage_order.size());
    render_passes.resize(m_stage_order.size());

    for (std::uint32_t i = 0; i < frame_datas.size(); i++) {
        auto &frame_data = frame_datas[i];
        frame_data.m_command_buffers.ensure_capacity(m_stage_order.size());
        frame_data.m_descriptor_sets.ensure_capacity(m_stage_order.size());
        frame_data.m_barriers.resize(m_stage_order.size());
        frame_data.m_framebuffers.resize(m_stage_order.size());
        frame_data.m_signal_semaphores.resize(m_stage_order.size());
        frame_data.m_wait_semaphores.resize(m_stage_order.size());
        frame_data.m_wait_stages.resize(m_stage_order.size());

        VkCommandPoolCreateInfo command_pool_ci{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = 0, // TODO: Don't hardcode.
        };
        ENSURE(vkCreateCommandPool(*device, &command_pool_ci, nullptr, &frame_data.m_command_pool) == VK_SUCCESS);

        VkCommandPoolCreateInfo transfer_pool_ci{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = 0, // TODO: Don't hardcode and use dedicated transfer queue.
        };
        ENSURE(vkCreateCommandPool(*device, &transfer_pool_ci, nullptr, &frame_data.m_transfer_pool) == VK_SUCCESS);

        VkCommandBufferAllocateInfo command_buffer_ai{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = frame_data.m_command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = m_stage_order.size(),
        };
        ENSURE(vkAllocateCommandBuffers(*device, &command_buffer_ai, frame_data.m_command_buffers.data()) ==
               VK_SUCCESS);

        VkCommandBufferAllocateInfo transfer_buffer_ai{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = frame_data.m_transfer_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        ENSURE(vkAllocateCommandBuffers(*device, &transfer_buffer_ai, &frame_data.m_transfer_buffer) == VK_SUCCESS);

        frame_data.m_sizes.resize(m_graph->resources().size());
        frame_data.m_memories.resize(m_graph->resources().size());
        frame_data.m_buffers.resize(m_graph->resources().size());
        frame_data.m_images.resize(m_graph->resources().size());
        frame_data.m_image_views.resize(m_graph->resources().size());
        frame_data.m_samplers.resize(m_graph->resources().size());
        frame_data.m_staging_memories.resize(m_graph->resources().size());
        frame_data.m_staging_buffers.resize(m_graph->resources().size());

        for (const auto &swapchain : m_graph->swapchains()) {
            frame_data.m_image_views[swapchain->m_index] = swapchain->m_swapchain.image_views()[i];
        }
    }

    std::unordered_map<const RenderResource *, Vector<const RenderStage *>> accessors;
    for (const auto *stage : m_stage_order) {
        for (const auto *resource : stage->m_reads) {
            accessors[resource].push(stage);
        }
        for (const auto *resource : stage->m_writes) {
            accessors[resource].push(stage);
        }
    }

    std::unordered_map<VkDescriptorType, std::uint32_t> descriptor_counts;
    for (const auto &buffer : m_graph->buffers()) {
        if (buffer->m_type == BufferType::IndexBuffer || buffer->m_type == BufferType::VertexBuffer) {
            continue;
        }
        descriptor_counts[descriptor_type(buffer->m_type)] += accessors[*buffer].size();
    }
    for (const auto &image : m_graph->images()) {
        descriptor_counts[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] += accessors[*image].size();
    }

    // Create descriptor pool.
    Vector<VkDescriptorPoolSize> descriptor_pool_sizes;
    descriptor_pool_sizes.ensure_capacity(descriptor_counts.size());
    for (auto [type, count] : descriptor_counts) {
        descriptor_pool_sizes.push(VkDescriptorPoolSize{
            .type = type,
            .descriptorCount = count,
        });
    }
    VkDescriptorPoolCreateInfo descriptor_pool_ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = m_stage_order.size(),
        .poolSizeCount = descriptor_pool_sizes.size(),
        .pPoolSizes = descriptor_pool_sizes.data(),
    };
    for (auto &frame_data : frame_datas) {
        ENSURE(vkCreateDescriptorPool(*device, &descriptor_pool_ci, nullptr, &frame_data.m_descriptor_pool) ==
               VK_SUCCESS);
    }

    // Create descriptor set layouts for each stage.
    for (const auto *stage : m_stage_order) {
        const VkShaderStageFlags stage_flags =
            stage->is<ComputeStage>() ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_ALL_GRAPHICS;
        Vector<VkDescriptorSetLayoutBinding> bindings;
        auto &binding_locations = resource_bindings[stage->m_index];
        bindings.ensure_capacity(m_graph->resources().size());
        binding_locations.resize(m_graph->resources().size());

        auto create_binding = [&](const RenderResource *resource) {
            VkDescriptorSetLayoutBinding binding{
                .binding = bindings.size(),
                .descriptorCount = 1,
                .stageFlags = stage_flags,
            };
            if (const auto *buffer = resource->as<BufferResource>()) {
                switch (buffer->m_type) {
                case BufferType::StorageBuffer:
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    break;
                case BufferType::UniformBuffer:
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    break;
                default:
                    return;
                }
            } else if (resource->is<ImageResource>()) {
                binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            }
            bindings.push(binding);

            Log::trace("render-graph", "%s -> %d (%s)", resource->m_name.c_str(), binding.binding,
                       stage->m_name.c_str());
            binding_locations[resource->m_index] = binding.binding;
        };
        for (const auto *resource : stage->m_reads) {
            create_binding(resource);
        }
        for (const auto *resource : stage->m_writes) {
            create_binding(resource);
        }

        VkDescriptorSetLayoutCreateInfo layout_ci{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = bindings.size(),
            .pBindings = bindings.data(),
        };
        ENSURE(vkCreateDescriptorSetLayout(*device, &layout_ci, nullptr, &descriptor_set_layouts[stage->m_index]) ==
               VK_SUCCESS);
    }

    // Allocate sets.
    for (auto &frame_data : frame_datas) {
        VkDescriptorSetAllocateInfo set_ai{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = frame_data.m_descriptor_pool,
            .descriptorSetCount = m_stage_order.size(),
            .pSetLayouts = descriptor_set_layouts.data(),
        };
        ENSURE(vkAllocateDescriptorSets(*device, &set_ai, frame_data.m_descriptor_sets.data()) == VK_SUCCESS);
    }

    // Create physical resources that are MemoryUsage::GpuOnly now.
    for (const auto &buffer : m_graph->buffers()) {
        if (buffer->m_usage == MemoryUsage::GpuOnly) {
            ASSERT(buffer->m_initial_size != 0);
            for (auto &frame_data : frame_datas) {
                frame_data.ensure_physical(*buffer, buffer->m_initial_size);
            }
        }
    }
    for (const auto &image : m_graph->images()) {
        ASSERT(image->m_usage == MemoryUsage::GpuOnly);
        for (auto &frame_data : frame_datas) {
            frame_data.ensure_physical(*image, 0);
        }
    }

    // Build render passes for graphics stages.
    // TODO: Redo transistions. Do them in a separate loop to avoid GraphicsStage cast when creating the render passes.
    Vector<VkImageLayout> image_layouts(m_graph->images().size() + m_graph->swapchains().size(),
                                        VK_IMAGE_LAYOUT_UNDEFINED);
    auto image_layout = [&](const ImageResource *image) -> VkImageLayout & {
        return image_layouts[image->m_index];
    };
    Vector<Vector<VkImageLayout>> stage_image_layouts(
        m_stage_order.size(), m_graph->images().size() + m_graph->swapchains().size(), VK_IMAGE_LAYOUT_UNDEFINED);
    auto stage_image_layout = [&](const RenderStage *stage, const ImageResource *image) -> VkImageLayout & {
        return stage_image_layouts[stage->m_index][image->m_index];
    };
    for (const auto *order : m_stage_order) {
        const auto *stage = order->as<GraphicsStage>();
        if (stage == nullptr) {
            continue;
        }
        Vector<VkAttachmentDescription> attachments;
        Vector<VkAttachmentReference> attachment_refs;
        auto &image_order = image_orders[stage->m_index];
        VkSubpassDescription subpass{
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        };
        VkAttachmentReference depth_reference{
            .attachment = 0,
        };
        for (const auto *input : stage->m_inputs) {
            image_order.push(input);
            if (input->m_type == ImageType::Depth) {
                attachments.push(VkAttachmentDescription{
                    .format = input->m_format,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = image_layout(input),
                    .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                });
                depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                image_layout(input) = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                stage_image_layout(stage, input) = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                subpass.pDepthStencilAttachment = &depth_reference;
                continue;
            }
            // TODO: Implement.
            ENSURE_NOT_REACHED();
        }

        ASSERT(!stage->m_outputs.empty());
        for (const auto *output : stage->m_outputs) {
            image_order.push(output);
            if (output->m_type == ImageType::Depth) {
                attachments.push(VkAttachmentDescription{
                    .format = output->m_format,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = image_layout(output),
                    .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                });
                depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                image_layout(output) = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                stage_image_layout(stage, output) = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                subpass.pDepthStencilAttachment = &depth_reference;
                continue;
            }
            VkImageLayout final_layout = output->m_type == ImageType::Swapchain
                                             ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                                             : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment_refs.push(VkAttachmentReference{
                .attachment = attachments.size(),
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            });
            attachments.push(VkAttachmentDescription{
                .format = output->m_format,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = image_layout(output),
                .finalLayout = final_layout,
            });
            image_layout(output) = final_layout;
            stage_image_layout(stage, output) = final_layout;
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
        ENSURE(vkCreateRenderPass(*device, &render_pass_ci, nullptr, &render_passes[stage->m_index]) == VK_SUCCESS);

        for (auto &frame_data : frame_datas) {
            Vector<VkImageView> framebuffer_attachments;
            framebuffer_attachments.ensure_capacity(image_order.size());
            for (const auto *image : image_order) {
                framebuffer_attachments.push(frame_data.m_image_views[image->m_index]);
            }
            VkFramebufferCreateInfo framebuffer_ci{
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = render_passes[stage->m_index],
                .attachmentCount = framebuffer_attachments.size(),
                .pAttachments = framebuffer_attachments.data(),
                .width = stage->m_outputs[0]->m_extent.width,
                .height = stage->m_outputs[0]->m_extent.height,
                .layers = 1,
            };
            if (!framebuffer_attachments.empty()) {
                ENSURE(vkCreateFramebuffer(*device, &framebuffer_ci, nullptr,
                                           &frame_data.m_framebuffers[stage->m_index]) == VK_SUCCESS);
            }
        }
    }

    // Create pipeline layouts.
    for (const auto *stage : m_stage_order) {
        VkPipelineLayoutCreateInfo pipeline_layout_ci{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &descriptor_set_layouts[stage->m_index],
            .pushConstantRangeCount = stage->m_push_constant_ranges.size(),
            .pPushConstantRanges = stage->m_push_constant_ranges.data(),
        };
        ENSURE(vkCreatePipelineLayout(*device, &pipeline_layout_ci, nullptr, &pipeline_layouts[stage->m_index]) ==
               VK_SUCCESS);
    }

    // Build pipelines.
    for (const auto &stage : m_graph->compute_stages()) {
        build_compute_pipeline(device, *stage, pipeline_layouts[stage->m_index], &pipelines[stage->m_index]);
    }
    for (const auto &stage : m_graph->graphics_stages()) {
        build_graphics_pipeline(device, *stage, pipeline_layouts[stage->m_index], render_passes[stage->m_index],
                                &pipelines[stage->m_index]);
    }

    for (VkImageLayout set_layout = VK_IMAGE_LAYOUT_UNDEFINED; auto &layouts : stage_image_layouts) {
        for (VkImageLayout &layout : layouts) {
            if (layout != VK_IMAGE_LAYOUT_UNDEFINED) {
                set_layout = layout;
            }
            layout = set_layout;
        }
    }

    // Build physical barriers.
    for (const auto &barrier : m_barriers) {
        for (auto &frame_data : frame_datas) {
            auto &physical_barrier = frame_data.m_barriers[barrier.dst()->m_index];
            physical_barrier.src = barrier_stage(barrier.src(), barrier.resource());
            physical_barrier.dst = barrier_stage(barrier.dst(), barrier.resource());
            if (const auto *buffer = barrier.resource()->as<BufferResource>()) {
                physical_barrier.buffers.push(VkBufferMemoryBarrier{
                    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                    .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                    .dstAccessMask = buffer->m_type == BufferType::UniformBuffer ? VK_ACCESS_UNIFORM_READ_BIT
                                                                                 : VK_ACCESS_SHADER_READ_BIT,
                    .buffer = frame_data.m_buffers[buffer->m_index],
                    .size = VK_WHOLE_SIZE,
                });
            } else if (const auto *image = barrier.resource()->as<ImageResource>()) {
                ASSERT(image->m_type != ImageType::Swapchain);
                physical_barrier.images.push(VkImageMemoryBarrier{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .srcAccessMask = image->m_type == ImageType::Depth ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
                                                                       : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                    .oldLayout = stage_image_layout(barrier.src(), image),
                    .newLayout = stage_image_layout(barrier.dst(), image),
                    .image = frame_data.m_images[image->m_index],
                    .subresourceRange{
                        .aspectMask =
                            image->m_type == ImageType::Depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
                        .levelCount = 1,
                        .layerCount = 1,
                    },
                });
            }
        }
    }
    return executable_graph;
}

// NOLINTNEXTLINE
void FrameData::ensure_physical(const RenderResource *resource, VkDeviceSize size) {
    if (const auto *image = resource->as<ImageResource>()) {
        auto &physical_image = m_images[image->m_index];
        auto &physical_image_view = m_image_views[image->m_index];
        auto &physical_sampler = m_samplers[image->m_index];
        auto &physical_memory = m_memories[image->m_index];

        VkImageUsageFlags usage = 0;
        if (image->m_type == ImageType::Depth) {
            usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }
        for (const auto *stage : m_compiled_graph->stage_order()) {
            if (std::find(stage->m_reads.begin(), stage->m_reads.end(), image) != stage->m_reads.end()) {
                usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
            }
        }
        VkImageCreateInfo image_ci{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = image->m_format,
            .extent = image->m_extent,
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        ENSURE(vkCreateImage(*m_device, &image_ci, nullptr, &physical_image) == VK_SUCCESS);

        VkMemoryRequirements memory_requirements;
        vkGetImageMemoryRequirements(*m_device, physical_image, &memory_requirements);

        physical_memory =
            m_device.allocate_memory(memory_requirements, MemoryType::GpuOnly, true, nullptr, physical_image);
        ENSURE(vkBindImageMemory(*m_device, physical_image, physical_memory, 0) == VK_SUCCESS);

        VkImageViewCreateInfo image_view_ci{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = physical_image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = image->m_format,
            .subresourceRange{
                .aspectMask = image->m_type == ImageType::Depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        ENSURE(vkCreateImageView(*m_device, &image_view_ci, nullptr, &physical_image_view) == VK_SUCCESS);

        // TODO: Don't always create a sampler.
        VkSamplerCreateInfo sampler_ci{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_NEAREST,
            .minFilter = VK_FILTER_NEAREST,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
        };
        ENSURE(vkCreateSampler(*m_device, &sampler_ci, nullptr, &physical_sampler) == VK_SUCCESS);

        // TODO: Insert transition if needed.
        VkDescriptorImageInfo image_info{
            .sampler = m_samplers[image->m_index],
            .imageView = m_image_views[image->m_index],
        };
        for (const auto *stage : m_compiled_graph->stage_order()) {
            if (std::find(stage->m_reads.begin(), stage->m_reads.end(), image) == stage->m_reads.end() &&
                std::find(stage->m_writes.begin(), stage->m_writes.end(), image) == stage->m_writes.end()) {
                continue;
            }
            bool is_write = std::find(stage->m_writes.begin(), stage->m_writes.end(), image) != stage->m_writes.end();
            switch (image->m_type) {
            case ImageType::Depth:
                image_info.imageLayout = is_write ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
                                                  : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                break;
            case ImageType::Normal:
                image_info.imageLayout =
                    is_write ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                break;
            default:
                ENSURE_NOT_REACHED();
            }
            VkWriteDescriptorSet write{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_descriptor_sets[stage->m_index],
                .dstBinding = m_executable_graph->m_resource_bindings[stage->m_index][image->m_index],
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &image_info,
            };
            vkUpdateDescriptorSets(*m_device, 1, &write, 0, nullptr);
        }
    }
    if (m_sizes[resource->m_index] >= size) {
        return;
    }
    m_sizes[resource->m_index] = size;
    if (const auto *buffer = resource->as<BufferResource>()) {
        auto &physical_buffer = m_buffers[buffer->m_index];
        auto &physical_memory = m_memories[buffer->m_index];

        // Destroy old buffer.
        vkDestroyBuffer(*m_device, physical_buffer, nullptr);
        vkFreeMemory(*m_device, physical_memory, nullptr);

        // Create new buffer with required size.
        VkBufferUsageFlags usage = buffer_usage(buffer->m_type);
        if (buffer->m_usage == MemoryUsage::TransferOnce) {
            usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }
        VkBufferCreateInfo buffer_ci{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        ENSURE(vkCreateBuffer(*m_device, &buffer_ci, nullptr, &physical_buffer) == VK_SUCCESS);

        VkMemoryRequirements memory_requirements;
        vkGetBufferMemoryRequirements(*m_device, physical_buffer, &memory_requirements);

        physical_memory = m_device.allocate_memory(
            memory_requirements,
            buffer->m_usage == MemoryUsage::HostVisible ? MemoryType::CpuToGpu : MemoryType::GpuOnly,
            buffer->m_usage != MemoryUsage::HostVisible, physical_buffer, nullptr);
        ENSURE(vkBindBufferMemory(*m_device, physical_buffer, physical_memory, 0) == VK_SUCCESS);

        // Index and vertex buffers never have associated descriptors.
        if (buffer->m_type == BufferType::IndexBuffer || buffer->m_type == BufferType::VertexBuffer) {
            return;
        }

        // Update descriptor.
        VkDescriptorBufferInfo buffer_info{
            .buffer = m_buffers[buffer->m_index],
            .range = VK_WHOLE_SIZE,
        };
        for (const auto *stage : m_compiled_graph->stage_order()) {
            if (std::find(stage->m_reads.begin(), stage->m_reads.end(), buffer) == stage->m_reads.end() &&
                std::find(stage->m_writes.begin(), stage->m_writes.end(), buffer) == stage->m_writes.end()) {
                continue;
            }
            VkWriteDescriptorSet write{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_descriptor_sets[stage->m_index],
                .dstBinding = m_executable_graph->m_resource_bindings[stage->m_index][buffer->m_index],
                .descriptorCount = 1,
                .descriptorType = descriptor_type(buffer->m_type),
                .pBufferInfo = &buffer_info,
            };
            vkUpdateDescriptorSets(*m_device, 1, &write, 0, nullptr);
        }
    }
}

FrameData::~FrameData() {
    for (auto *staging_buffer : m_staging_buffers) {
        vkDestroyBuffer(*m_device, staging_buffer, nullptr);
    }
    for (auto *staging_memory : m_staging_memories) {
        vkFreeMemory(*m_device, staging_memory, nullptr);
    }
    for (auto *sampler : m_samplers) {
        vkDestroySampler(*m_device, sampler, nullptr);
    }
    for (std::uint32_t i = 0; auto *image_view : m_image_views) {
        if (const auto *image = m_graph->resources()[i++]->as<ImageResource>()) {
            if (image->type() != ImageType::Swapchain) {
                vkDestroyImageView(*m_device, image_view, nullptr);
            }
        }
    }
    for (auto *image : m_images) {
        vkDestroyImage(*m_device, image, nullptr);
    }
    for (auto *buffer : m_buffers) {
        vkDestroyBuffer(*m_device, buffer, nullptr);
    }
    for (auto *memory : m_memories) {
        vkFreeMemory(*m_device, memory, nullptr);
    }
    for (auto *framebuffer : m_framebuffers) {
        vkDestroyFramebuffer(*m_device, framebuffer, nullptr);
    }
    vkDestroyDescriptorPool(*m_device, m_descriptor_pool, nullptr);
    vkDestroyCommandPool(*m_device, m_transfer_pool, nullptr);
    vkDestroyCommandPool(*m_device, m_command_pool, nullptr);
}

void FrameData::insert_signal_semaphore(const RenderStage *stage, VkSemaphore semaphore) {
    m_signal_semaphores[stage->m_index].push(semaphore);
}

void FrameData::insert_wait_semaphore(const RenderStage *stage, VkSemaphore semaphore,
                                      VkPipelineStageFlags wait_stage) {
    m_wait_semaphores[stage->m_index].push(semaphore);
    m_wait_stages[stage->m_index].push(wait_stage);
}

void FrameData::transfer(const RenderResource *resource, const void *data, VkDeviceSize size) {
    const auto *buffer = resource->as<BufferResource>();
    ASSERT(buffer != nullptr && buffer->m_usage == MemoryUsage::TransferOnce);
    ensure_physical(resource, size);

    // Create staging buffer.
    auto &physical_buffer = m_staging_buffers[buffer->m_index];
    auto &physical_memory = m_staging_memories[buffer->m_index];
    VkBufferCreateInfo buffer_ci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    ENSURE(vkCreateBuffer(*m_device, &buffer_ci, nullptr, &physical_buffer) == VK_SUCCESS);

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(*m_device, physical_buffer, &memory_requirements);

    physical_memory =
        m_device.allocate_memory(memory_requirements, MemoryType::CpuToGpu, false, physical_buffer, nullptr);
    ENSURE(vkBindBufferMemory(*m_device, physical_buffer, physical_memory, 0) == VK_SUCCESS);

    void *staging_data;
    vkMapMemory(*m_device, m_staging_memories[buffer->m_index], 0, VK_WHOLE_SIZE, 0, &staging_data);
    std::memcpy(staging_data, data, size);
    vkUnmapMemory(*m_device, m_staging_memories[buffer->m_index]);

    m_transfer_queue.push(Transfer{
        .src = m_staging_buffers[buffer->m_index],
        .dst = m_buffers[buffer->m_index],
        .size = size,
    });

    // TODO: Delete staging buffer.
}

void FrameData::upload(const RenderResource *resource, const void *data, VkDeviceSize size, VkDeviceSize offset) {
    ASSERT(resource->m_usage == MemoryUsage::HostVisible);
    ensure_physical(resource, size);
    void *mapped_data;
    vkMapMemory(*m_device, m_memories[resource->m_index], 0, VK_WHOLE_SIZE, 0, &mapped_data);
    std::memcpy(static_cast<char *>(mapped_data) + offset, data, size);
    vkUnmapMemory(*m_device, m_memories[resource->m_index]);
}

void ExecutableGraph::record_compute_commands(const ComputeStage *stage, FrameData &frame_data) {
    auto *cmd_buf = frame_data.m_command_buffers[stage->m_index];
    VkCommandBufferBeginInfo cmd_buf_bi{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmd_buf, &cmd_buf_bi);

    const auto &barrier = frame_data.m_barriers[stage->m_index];
    if (barrier.dst != 0) {
        vkCmdPipelineBarrier(cmd_buf, barrier.src, barrier.dst, 0, 0, nullptr, barrier.buffers.size(),
                             barrier.buffers.data(), barrier.images.size(), barrier.images.data());
    }

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layouts[stage->m_index], 0, 1,
                            &frame_data.m_descriptor_sets[stage->m_index], 0, nullptr);
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelines[stage->m_index]);
    stage->m_on_record(cmd_buf, m_pipeline_layouts[stage->m_index]);
    vkEndCommandBuffer(cmd_buf);
}

void ExecutableGraph::record_graphics_commands(const GraphicsStage *stage, FrameData &frame_data) {
    auto *cmd_buf = frame_data.m_command_buffers[stage->m_index];
    VkCommandBufferBeginInfo cmd_buf_bi{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmd_buf, &cmd_buf_bi);

    const auto &barrier = frame_data.m_barriers[stage->m_index];
    if (barrier.dst != 0) {
        vkCmdPipelineBarrier(cmd_buf, barrier.src, barrier.dst, 0, 0, nullptr, barrier.buffers.size(),
                             barrier.buffers.data(), barrier.images.size(), barrier.images.data());
    }

    Vector<VkClearValue> clear_values;
    clear_values.ensure_capacity(m_image_orders[stage->m_index].size());
    for (const auto *image : m_image_orders[stage->m_index]) {
        clear_values.push(image->m_clear_value);
    }

    VkRenderPassBeginInfo render_pass_bi{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = m_render_passes[stage->m_index],
        .framebuffer = frame_data.m_framebuffers[stage->m_index],
        .renderArea{.extent{stage->m_outputs[0]->m_extent.width, stage->m_outputs[0]->m_extent.height}},
        .clearValueCount = clear_values.size(),
        .pClearValues = clear_values.data(),
    };
    vkCmdBeginRenderPass(cmd_buf, &render_pass_bi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layouts[stage->m_index], 0, 1,
                            &frame_data.m_descriptor_sets[stage->m_index], 0, nullptr);
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines[stage->m_index]);

    Vector<VkBuffer> vertex_buffers;
    for (const auto *resource : stage->m_reads) {
        const auto *buffer = resource->as<BufferResource>();
        if (buffer == nullptr) {
            continue;
        }

        auto *physical_buffer = frame_data.m_buffers[buffer->m_index];
        if (buffer->m_type == BufferType::IndexBuffer) {
            // TODO: Get index type from buffer resource.
            vkCmdBindIndexBuffer(cmd_buf, physical_buffer, 0, VK_INDEX_TYPE_UINT32);
        } else if (buffer->m_type == BufferType::VertexBuffer) {
            vertex_buffers.push(physical_buffer);
        }
    }

    Vector<VkDeviceSize> offsets;
    for (std::uint32_t i = 0; i < vertex_buffers.size(); i++) {
        offsets.push(0);
    }
    vkCmdBindVertexBuffers(cmd_buf, 0, vertex_buffers.size(), vertex_buffers.data(), offsets.data());
    stage->m_on_record(cmd_buf, m_pipeline_layouts[stage->m_index]);
    vkCmdEndRenderPass(cmd_buf);
    vkEndCommandBuffer(cmd_buf);
}

ExecutableGraph::~ExecutableGraph() {
    for (auto *descriptor_set_layout : m_descriptor_set_layouts) {
        vkDestroyDescriptorSetLayout(*m_device, descriptor_set_layout, nullptr);
    }
    for (auto *pipeline : m_pipelines) {
        vkDestroyPipeline(*m_device, pipeline, nullptr);
    }
    for (auto *pipeline_layout : m_pipeline_layouts) {
        vkDestroyPipelineLayout(*m_device, pipeline_layout, nullptr);
    }
    for (auto *render_pass : m_render_passes) {
        vkDestroyRenderPass(*m_device, render_pass, nullptr);
    }
}

void ExecutableGraph::render(std::uint32_t frame_index, VkQueue queue, VkFence signal_fence) {
    auto &frame_data = m_frame_datas[frame_index % m_frame_datas.size()];

    // Execute transfers.
    if (!frame_data.m_transfer_queue.empty()) {
        vkResetCommandPool(*m_device, frame_data.m_transfer_pool, 0);
        VkCommandBufferBeginInfo transfer_buffer_bi{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(frame_data.m_transfer_buffer, &transfer_buffer_bi);
        while (!frame_data.m_transfer_queue.empty()) {
            auto transfer = frame_data.m_transfer_queue.pop();
            VkBufferCopy copy{
                .srcOffset = 0,
                .dstOffset = 0,
                .size = transfer.size,
            };
            vkCmdCopyBuffer(frame_data.m_transfer_buffer, transfer.src, transfer.dst, 1, &copy);
        }
        VkMemoryBarrier barrier{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        };
        vkCmdPipelineBarrier(frame_data.m_transfer_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
        vkEndCommandBuffer(frame_data.m_transfer_buffer);
        VkSubmitInfo transfer_si{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &frame_data.m_transfer_buffer,
        };
        vkQueueSubmit(queue, 1, &transfer_si, nullptr);
    }

    // Reset and re-record command buffers.
    vkResetCommandPool(*m_device, frame_data.m_command_pool, 0);
    for (const auto &stage : m_graph->compute_stages()) {
        record_compute_commands(*stage, frame_data);
    }
    for (const auto &stage : m_graph->graphics_stages()) {
        record_graphics_commands(*stage, frame_data);
    }

    m_submit_infos.clear();
    for (const auto *stage : m_compiled_graph->stage_order()) {
        const auto &signal_semaphores = frame_data.m_signal_semaphores[stage->m_index];
        const auto &wait_semaphores = frame_data.m_wait_semaphores[stage->m_index];
        m_submit_infos.push(VkSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = wait_semaphores.size(),
            .pWaitSemaphores = wait_semaphores.data(),
            .pWaitDstStageMask = frame_data.m_wait_stages[stage->m_index].data(),
            .commandBufferCount = 1,
            .pCommandBuffers = &frame_data.m_command_buffers[stage->m_index],
            .signalSemaphoreCount = signal_semaphores.size(),
            .pSignalSemaphores = signal_semaphores.data(),
        });
    }
    vkQueueSubmit(queue, m_submit_infos.size(), m_submit_infos.data(), signal_fence);
}

FrameData &ExecutableGraph::frame_data(std::uint32_t index) {
    return m_frame_datas[index % m_frame_datas.size()];
}

// NOLINTNEXTLINE
std::string CompiledGraph::to_dot() const {
    std::stringstream ss;
    ss << "digraph {\n";
    ss << "    rankdir = LR;\n";
    ss << "    node [shape=box];\n";

    std::size_t count = 0;
    std::size_t subgraph_count = 0;
    auto output_cluster = [&](const std::string &title, bool output_linearisation, bool output_barriers,
                              bool output_semaphores) {
        std::unordered_map<const void *, std::size_t> id_map;
        auto unique_id = [&](const void *obj) {
            if (!id_map.contains(obj)) {
                id_map.emplace(obj, count++);
            }
            return id_map.at(obj);
        };
        ss << "    subgraph cluster_" << std::to_string(subgraph_count++) << " {\n";
        ss << "        label = \"" << title << "\";\n";
        for (const auto *resource : m_graph->resources()) {
            ss << "        " << unique_id(resource) << " [label=\"" << resource->m_name << "\", color=blue];\n";
        }

        for (const auto *stage : m_stage_order) {
            ss << "        " << unique_id(stage) << " [label=\"" << stage->m_name << "\", color=red];\n";
            for (const auto *resource : stage->m_writes) {
                ss << "        " << unique_id(stage) << " -> " << unique_id(resource) << " [color=orange];\n";
            }
            for (const auto *resource : stage->m_reads) {
                ss << "        " << unique_id(resource) << " -> " << unique_id(stage) << " [color=orange];\n";
            }
        }
        for (const auto &stage : m_graph->graphics_stages()) {
            for (const auto *resource : stage->m_outputs) {
                ss << "        " << unique_id(*stage) << " -> " << unique_id(resource) << " [color=deeppink];\n";
            }
            for (const auto *resource : stage->m_inputs) {
                ss << "        " << unique_id(resource) << " -> " << unique_id(*stage) << " [color=deeppink];\n";
            }
        }

        if (output_linearisation) {
            for (const RenderStage *connect_stage = nullptr; const auto *stage : m_stage_order) {
                if (connect_stage != nullptr) {
                    ss << "        " << unique_id(connect_stage) << " -> " << unique_id(stage)
                       << " [color=black,penwidth=2];\n";
                }
                connect_stage = stage;
            }
        }

        if (output_barriers) {
            for (const auto &barrier : m_barriers) {
                ss << "        " << unique_id(&barrier) << " [label=\"Barrier\"];\n";
                ss << "        " << unique_id(barrier.src()) << " -> " << unique_id(&barrier) << " [color=red];\n";
                ss << "        " << unique_id(barrier.resource()) << " -> " << unique_id(&barrier)
                   << " [color=orange];\n";
                ss << "        " << unique_id(&barrier) << " -> " << unique_id(barrier.dst()) << " [color=red];\n";
            }
        }

        if (output_semaphores) {
            for (const auto &semaphore : m_semaphores) {
                ss << "        " << unique_id(semaphore.signaller()) << " -> " << unique_id(semaphore.waiter())
                   << " [color=green4];\n";
            }
        }
        ss << "    }\n";
    };

    if (!m_semaphores.empty()) {
        output_cluster("Semaphore Insertion (" + std::to_string(m_semaphores.size()) + ")", true, true, true);
    }
    if (!m_barriers.empty()) {
        output_cluster("Barrier Insertion (" + std::to_string(m_barriers.size()) + ")", true, true, false);
    }
    output_cluster("Linearisation", true, false, false);
    output_cluster("Input", false, false, false);

    // Build key.
    ss << R"end(
    subgraph cluster_key {
        label = "Key";
        node [shape=plaintext];

        k1a [label="Barrier", width=2];
        k1b [style="invisible"];
        k1a -> k1b [color=red];

        k2a [label="Linear order", width=2];
        k2b [style="invisible"];
        k2a -> k2b [color=black,penwidth=2];

        k3a [label="Resource access (normal)", width=2];
        k3b [style="invisible"];
        k3a -> k3b [color=orange];

        k4a [label="Resource access (attachment)", width=2];
        k4b [style="invisible"];
        k4a -> k4b [color=deeppink];

        k5a [label="Semaphore", width=2];
        k5b [style="invisible"];
        k5a -> k5b [color=green4];
    })end";

    ss << "\n}\n";
    return ss.str();
}
