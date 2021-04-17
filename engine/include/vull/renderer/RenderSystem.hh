#pragma once

#include <vull/core/System.hh>
#include <vull/renderer/Buffer.hh>
#include <vull/renderer/Image.hh>
#include <vull/renderer/PointLight.hh> // IWYU pragma: keep
#include <vull/renderer/UniformBuffer.hh>
#include <vull/renderer/Vertex.hh> // IWYU pragma: keep
#include <vull/support/Vector.hh>

#include <vulkan/vulkan_core.h>

#include <cstdint>

class Device;
class Swapchain;
class Window;
struct World;

class RenderSystem final : public System<RenderSystem> {
    const Device &m_device;
    const Swapchain &m_swapchain;
    const Window &m_window;
    std::uint32_t m_row_tile_count;
    std::uint32_t m_col_tile_count;

    VkCommandPool m_command_pool{nullptr};
    VkDescriptorPool m_descriptor_pool{nullptr};

    VkQueue m_compute_queue{nullptr};
    VkQueue m_graphics_queue{nullptr};

    VkRenderPass m_depth_pass_render_pass{nullptr};
    VkRenderPass m_main_pass_render_pass{nullptr};

    VkShaderModule m_depth_pass_vertex_shader{nullptr};
    VkShaderModule m_light_cull_pass_compute_shader{nullptr};
    VkShaderModule m_main_pass_vertex_shader{nullptr};
    VkShaderModule m_main_pass_fragment_shader{nullptr};

    Buffer m_vertex_buffer;
    Buffer m_index_buffer;
    Buffer m_lights_buffer;
    Buffer m_light_visibilities_buffer;
    Buffer m_uniform_buffer;

    Image m_depth_image;
    VkImageView m_depth_image_view{nullptr};
    VkFramebuffer m_depth_framebuffer{nullptr};
    VkSampler m_depth_sampler{nullptr};

    VkDescriptorSetLayout m_lights_set_layout{nullptr};
    VkDescriptorSetLayout m_ubo_set_layout{nullptr};
    VkDescriptorSetLayout m_depth_sampler_set_layout{nullptr};
    VkDescriptorSet m_lights_set{nullptr};
    VkDescriptorSet m_ubo_set{nullptr};
    VkDescriptorSet m_depth_sampler_set{nullptr};

    VkPipelineLayout m_depth_pass_pipeline_layout{nullptr};
    VkPipelineLayout m_light_cull_pass_pipeline_layout{nullptr};
    VkPipelineLayout m_main_pass_pipeline_layout{nullptr};
    VkPipeline m_depth_pass_pipeline{nullptr};
    VkPipeline m_light_cull_pass_pipeline{nullptr};
    VkPipeline m_main_pass_pipeline{nullptr};

    Vector<VkFramebuffer> m_output_framebuffers;
    Vector<VkCommandBuffer> m_command_buffers;

    VkFence m_frame_finished{nullptr};
    VkSemaphore m_image_available{nullptr};
    VkSemaphore m_depth_pass_finished{nullptr};
    VkSemaphore m_light_cull_pass_finished{nullptr};
    VkSemaphore m_main_pass_finished{nullptr};

    Vector<PointLight> m_lights;
    UniformBuffer m_ubo{};
    void *m_lights_data{nullptr};
    void *m_ubo_data{nullptr};

    void create_pools();
    void create_queues();
    void create_render_passes();
    void load_shaders();
    void create_data_buffers(const Vector<Vertex> &vertices, const Vector<std::uint32_t> &indices);
    void create_depth_buffer();
    void create_descriptors();
    void create_pipeline_layouts();
    void create_pipelines();
    void create_output_buffers();
    void allocate_command_buffers();
    void record_command_buffers(World *world);
    void create_sync_objects();

public:
    RenderSystem(const Device &device, const Swapchain &swapchain, const Window &window, const Vector<Vertex> &vertices,
                 const Vector<std::uint32_t> &indices);
    RenderSystem(const RenderSystem &) = delete;
    RenderSystem(RenderSystem &&) = delete;
    ~RenderSystem() override;

    RenderSystem &operator=(const RenderSystem &) = delete;
    RenderSystem &operator=(RenderSystem &&) = delete;

    void update(World *world, float dt) override;

    Vector<PointLight> &lights() { return m_lights; }
    UniformBuffer &ubo() { return m_ubo; }
};
