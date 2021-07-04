#include <vull/rendering/ComputeStage.hh>

#include <vull/support/Vector.hh>
#include <vull/vulkan/Device.hh>
#include <vull/vulkan/Shader.hh>

ComputeStage::~ComputeStage() {
    vkDestroyPipeline(**m_device, m_pipeline, nullptr);
}

void ComputeStage::build_objects(const Device &device, ExecutableGraph *executable_graph) {
    RenderStage::build_objects(device, executable_graph);

    const auto &shader = *m_shaders[0];
    ASSERT(shader.stage() == VK_SHADER_STAGE_COMPUTE_BIT);
    VkPipelineShaderStageCreateInfo shader_stage_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = *shader,
        .pName = "main",
        .pSpecializationInfo = &m_specialisation_info,
    };
    VkComputePipelineCreateInfo pipeline_ci{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = shader_stage_ci,
        .layout = m_pipeline_layout,
    };
    ENSURE(vkCreateComputePipelines(*device, nullptr, 1, &pipeline_ci, nullptr, &m_pipeline) == VK_SUCCESS);
}

void ComputeStage::start_recording(VkCommandBuffer cmd_buf) const {
    RenderStage::start_recording(cmd_buf);
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
}

void ComputeStage::dispatch(std::uint32_t width, std::uint32_t height, std::uint32_t depth) {
    vkCmdDispatch(m_cmd_buf, width, height, depth);
}
