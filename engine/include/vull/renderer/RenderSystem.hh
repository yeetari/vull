#pragma once

#include <vull/core/System.hh>
#include <vull/renderer/PointLight.hh> // IWYU pragma: keep
#include <vull/renderer/RenderGraph.hh>
#include <vull/renderer/UniformBuffer.hh>
#include <vull/renderer/Vertex.hh> // IWYU pragma: keep
#include <vull/support/Box.hh>
#include <vull/support/Span.hh> // IWYU pragma: keep
#include <vull/support/Vector.hh>

#include <vulkan/vulkan_core.h>

#include <cstdint>

class Device;
class Swapchain;
struct World;

class RenderSystem final : public System<RenderSystem> {
    const Device &m_device;
    const Swapchain &m_swapchain;
    std::uint32_t m_row_tile_count;
    std::uint32_t m_col_tile_count;

    RenderGraph m_graph;
    Box<CompiledGraph> m_compiled_graph;
    Box<ExecutableGraph> m_executable_graph;
    BufferResource *m_light_buffer;
    BufferResource *m_uniform_buffer;
    GraphicsStage *m_depth_pass;
    GraphicsStage *m_main_pass;

    std::uint32_t m_frame_index{0};
    VkQueue m_queue{nullptr};

    Vector<VkFence> m_frame_fences;
    Vector<VkSemaphore> m_image_available_semaphores;
    Vector<VkSemaphore> m_rendering_finished_semaphores;

    Vector<PointLight> m_lights;
    UniformBuffer m_ubo{};

    void create_queue();
    void create_sync_objects();

public:
    RenderSystem(const Device &device, const Swapchain &swapchain, Span<std::uint8_t> vertices,
                 Span<std::uint8_t> indices);
    RenderSystem(const RenderSystem &) = delete;
    RenderSystem(RenderSystem &&) = delete;
    ~RenderSystem() override;

    RenderSystem &operator=(const RenderSystem &) = delete;
    RenderSystem &operator=(RenderSystem &&) = delete;

    void update(World *world, float dt) override;

    Vector<PointLight> &lights() { return m_lights; }
    UniformBuffer &ubo() { return m_ubo; }
};
