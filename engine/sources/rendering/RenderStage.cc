#include <vull/rendering/RenderStage.hh>

#include <vull/rendering/ExecutableGraph.hh>
#include <vull/rendering/RenderResource.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Log.hh>
#include <vull/vulkan/Device.hh>
#include <vull/vulkan/Semaphore.hh>
#include <vull/vulkan/Shader.hh>

#include <algorithm>
#include <cstdlib>

RenderStage::~RenderStage() {
    vkDestroyPipelineLayout(**m_device, m_pipeline_layout, nullptr);
}

void RenderStage::add_shader(const Shader &shader) {
    m_shaders.push(&shader);
    if (shader.push_constant_size() != 0) {
        m_push_constant_stages |= shader.stage();
    }
}

void RenderStage::set_constant(std::string name, std::size_t value) {
    m_specialisation_constants[std::move(name)] = value;
}

void RenderStage::reads_from(RenderResource *resource) {
    m_reads.push(resource);
    resource->m_readers.push(this);
}

void RenderStage::writes_to(RenderResource *resource) {
    m_writes.push(resource);
    resource->m_writers.push(this);
}

void RenderStage::add_signal_semaphore(std::uint32_t frame_index, const Semaphore &semaphore) {
    m_signal_semaphores[frame_index].push(*semaphore);
}

void RenderStage::add_wait_semaphore(std::uint32_t frame_index, const Semaphore &semaphore,
                                     VkPipelineStageFlags wait_stage) {
    m_wait_semaphores[frame_index].push(*semaphore);
    if (frame_index == 0) {
        m_wait_stages.push(wait_stage);
    }
}

void RenderStage::set_initial_layout(const RenderTexture *texture, VkImageLayout layout) {
    [[maybe_unused]] bool inserted = m_initial_layouts.emplace(texture, layout).second;
    ASSERT(inserted);
}

void RenderStage::set_final_layout(const RenderTexture *texture, VkImageLayout layout) {
    [[maybe_unused]] bool inserted = m_final_layouts.emplace(texture, layout).second;
    ASSERT(inserted);
}

void RenderStage::build_objects(const Device &device, ExecutableGraph *executable_graph) {
    m_device = &device;
    m_signal_semaphores.resize(executable_graph->frame_queue_length());
    m_wait_semaphores.resize(executable_graph->frame_queue_length());
    m_wait_stages.ensure_capacity(executable_graph->frame_queue_length());

    // TODO
    Vector<const RenderStage *> stage_order;

    // Build barriers.
    const auto *our_pos = std::find(stage_order.begin(), stage_order.end(), this);
    for (const auto *resource : m_reads) {
        for (const auto *writer : resource->m_writers) {
            const auto *writer_pos = std::find(stage_order.begin(), stage_order.end(), writer);
            if (writer_pos > our_pos) {
                continue;
            }
            Log::warn("rendering", "Barrier from %s to %s (resource %s)", writer->m_name.c_str(), m_name.c_str(),
                      resource->name().c_str());
        }
    }

    // Create pipeline layout.
    std::uint32_t push_constant_size = 0;
    for (const auto *shader : m_shaders) {
        push_constant_size = std::max(push_constant_size, shader->push_constant_size());
    }
    VkPushConstantRange push_constant_range{
        .stageFlags = m_push_constant_stages,
        .size = push_constant_size,
    };
    VkPipelineLayoutCreateInfo pipeline_layout_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pSetLayouts = nullptr,
        .pushConstantRangeCount = push_constant_size != 0 ? 1u : 0u,
        .pPushConstantRanges = &push_constant_range,
    };
    ENSURE(vkCreatePipelineLayout(*device, &pipeline_layout_ci, nullptr, &m_pipeline_layout) == VK_SUCCESS);

    // Build specialisation info.
    for (const auto *shader : m_shaders) {
        for (const auto &constant_info : shader->specialisation_constants()) {
            if (!m_specialisation_constants.contains(constant_info.name)) {
                Log::error("rendering", "Missing value for specialisation constant %s in stage %s",
                           constant_info.name.c_str(), m_name.c_str());
                std::exit(1);
            }
            if (std::find_if(m_specialisation_map_entries.begin(), m_specialisation_map_entries.end(),
                             [&constant_info](const VkSpecializationMapEntry &entry) {
                                 return constant_info.id == entry.constantID;
                             }) != m_specialisation_map_entries.end()) {
                continue;
            }
            std::size_t value = m_specialisation_constants[constant_info.name];
            m_specialisation_map_entries.push(VkSpecializationMapEntry{
                .constantID = constant_info.id,
                .offset = m_specialisation_values.size_bytes(),
                .size = constant_info.size,
            });
            m_specialisation_values.push(value);
        }
    }
    m_specialisation_info = {
        .mapEntryCount = m_specialisation_map_entries.size(),
        .pMapEntries = m_specialisation_map_entries.data(),
        .dataSize = m_specialisation_values.size_bytes(),
        .pData = m_specialisation_values.data(),
    };
}

void RenderStage::start_recording(VkCommandBuffer cmd_buf) const {
    m_cmd_buf = cmd_buf;
    VkCommandBufferBeginInfo cmd_buf_bi{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmd_buf, &cmd_buf_bi);

    // TODO: Barriers.
    // TODO: Bind descriptor sets.
}

VkCommandBuffer RenderStage::stop_recording() const {
    vkEndCommandBuffer(m_cmd_buf);
    return m_cmd_buf;
}

void RenderStage::push_constants(const void *data, std::uint32_t size) {
    ASSERT(m_push_constant_stages != 0);
    vkCmdPushConstants(m_cmd_buf, m_pipeline_layout, m_push_constant_stages, 0, size, data);
}
