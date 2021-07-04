#pragma once

#include <vull/rendering/RenderStage.hh>
#include <vull/support/Vector.hh>

#include <vulkan/vulkan_core.h>

// TODO: Potentially separate the pipeline and renderpass of GraphicsStage to allow for lots of pipelines for different
//       materials. Would also work for depth pass and shadow depth pass where the same pipeline is used, just different
//       outputs.

class RenderTexture;

class GraphicsStage final : public RenderStage {
    Vector<const RenderTexture *> m_inputs;
    Vector<const RenderTexture *> m_outputs;

    VkFramebuffer m_framebuffer{nullptr};
    VkRenderPass m_render_pass{nullptr};
    VkPipeline m_pipeline{nullptr};
    Vector<const RenderTexture *> m_texture_order;

public:
    explicit GraphicsStage(std::string &&name) : RenderStage(std::move(name)) {}
    GraphicsStage(const GraphicsStage &) = delete;
    GraphicsStage(GraphicsStage &&) = delete;
    ~GraphicsStage() override;

    GraphicsStage &operator=(const GraphicsStage &) = delete;
    GraphicsStage &operator=(GraphicsStage &&) = delete;

    void add_input(RenderTexture *texture);
    void add_output(RenderTexture *texture);

    void build_objects(const Device &device, ExecutableGraph *executable_graph) override;
    void start_recording(VkCommandBuffer cmd_buf) const override;
    VkCommandBuffer stop_recording() const override;

    void draw_indexed(std::uint32_t index_count, std::uint32_t first_index);

    const Vector<const RenderTexture *> &inputs() const { return m_inputs; }
    const Vector<const RenderTexture *> &outputs() const { return m_outputs; }
};
