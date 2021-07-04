#pragma once

#include <vull/rendering/RenderStage.hh>

#include <vulkan/vulkan_core.h>

class ComputeStage final : public RenderStage {
    VkPipeline m_pipeline{nullptr};

public:
    explicit ComputeStage(std::string &&name) : RenderStage(std::move(name)) {}
    ComputeStage(const ComputeStage &) = delete;
    ComputeStage(ComputeStage &&) = delete;
    ~ComputeStage() override;

    ComputeStage &operator=(const ComputeStage &) = delete;
    ComputeStage &operator=(ComputeStage &&) = delete;

    void build_objects(const Device &device, ExecutableGraph *executable_graph) override;
    void start_recording(VkCommandBuffer cmd_buf) const override;

    void dispatch(std::uint32_t width, std::uint32_t height, std::uint32_t depth);
};
