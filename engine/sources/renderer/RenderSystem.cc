#include <vull/renderer/RenderSystem.hh>

#include <vull/core/Entity.hh>
#include <vull/core/Transform.hh>
#include <vull/core/World.hh>
#include <vull/io/FileSystem.hh>
#include <vull/renderer/Material.hh>
#include <vull/renderer/Mesh.hh>
#include <vull/renderer/MeshOffset.hh>
#include <vull/renderer/PointLight.hh>
#include <vull/renderer/UniformBuffer.hh>
#include <vull/renderer/Vertex.hh>
#include <vull/rendering/GraphicsStage.hh>
#include <vull/rendering/RenderGraph.hh>
#include <vull/rendering/RenderIndexBuffer.hh>
#include <vull/rendering/RenderSwapchain.hh>
#include <vull/rendering/RenderTexture.hh>
#include <vull/rendering/RenderVertexBuffer.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Box.hh>
#include <vull/support/Log.hh>
#include <vull/support/Span.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/Device.hh>
#include <vull/vulkan/Fence.hh>
#include <vull/vulkan/Semaphore.hh>
#include <vull/vulkan/Shader.hh>
#include <vull/vulkan/Swapchain.hh>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <cstdint>
#include <string>

class Texture;

namespace {

constexpr int k_max_light_count = 6000;
constexpr int k_max_lights_per_tile = 400;
constexpr int k_tile_size = 32; // TODO: Configurable.

constexpr VkDeviceSize k_lights_buffer_size = k_max_light_count * sizeof(PointLight) + sizeof(glm::vec4);
constexpr VkDeviceSize k_light_visibility_size = k_max_lights_per_tile * sizeof(std::uint32_t) + sizeof(std::uint32_t);

} // namespace

RenderSystem::RenderSystem(const Device &device, const Swapchain &swapchain, Span<std::uint8_t> vertices,
                           Span<std::uint8_t> indices)
    : m_device(device), m_swapchain(swapchain) {
    auto swapchain_extent = swapchain.extent();
    m_row_tile_count = (swapchain_extent.width + (swapchain_extent.width % k_tile_size)) / k_tile_size;
    m_col_tile_count = (swapchain_extent.height + (swapchain_extent.height % k_tile_size)) / k_tile_size;

    Shader vertex_shader(device, FileSystem::load_shader("builtin/shaders/flat.vert"));
    Shader fragment_shader(device, FileSystem::load_shader("builtin/shaders/flat.frag"));

    m_back_buffer = m_graph.add<RenderSwapchain>(swapchain);
    m_back_buffer->set_clear_colour(0.4f, 0.6f, 0.7f, 1.0f);

    auto *depth_buffer = m_graph.add<RenderTexture>(TextureType::Depth, MemoryUsage::GpuOnly);
    depth_buffer->set_clear_depth_stencil(1.0f, 0);
    depth_buffer->set_extent(swapchain.extent());
    depth_buffer->set_format(VK_FORMAT_D32_SFLOAT);

    auto *index_buffer = m_graph.add<RenderIndexBuffer>(MemoryUsage::Transfer, IndexType::UInt32);
    auto *vertex_buffer = m_graph.add<RenderVertexBuffer>(MemoryUsage::Transfer, sizeof(Vertex));
    vertex_buffer->add_attribute(VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position));
    vertex_buffer->add_attribute(VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal));
    vertex_buffer->add_attribute(VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv));

    m_depth_pass = m_graph.add<GraphicsStage>("depth pass");
    m_depth_pass->add_output(depth_buffer);
    m_depth_pass->add_shader(vertex_shader);
    m_depth_pass->reads_from(index_buffer);
    m_depth_pass->reads_from(vertex_buffer);

    m_main_pass = m_graph.add<GraphicsStage>("main pass");
    m_main_pass->add_input(depth_buffer);
    m_main_pass->add_output(m_back_buffer);
    m_main_pass->add_shader(vertex_shader);
    m_main_pass->add_shader(fragment_shader);
    m_main_pass->reads_from(index_buffer);
    m_main_pass->reads_from(vertex_buffer);

    m_compiled_graph = m_graph.compile(m_back_buffer);
    m_executable_graph = m_compiled_graph->build_objects(device, swapchain.image_count());

    // TODO: Very sad, might have to make virtual transfer in MemoryResource protected.
    index_buffer->MemoryResource::transfer(indices);
    vertex_buffer->MemoryResource::transfer(vertices);

    m_frame_fences.resize(m_swapchain.image_count(), m_device, true);
    m_image_available_semaphores.resize(m_swapchain.image_count(), m_device);
    m_rendering_finished_semaphores.resize(m_swapchain.image_count(), m_device);
    for (std::uint32_t i = 0; i < m_executable_graph->frame_queue_length(); i++) {
        m_main_pass->add_signal_semaphore(i, m_rendering_finished_semaphores[i]);
        m_main_pass->add_wait_semaphore(i, m_image_available_semaphores[i],
                                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    }
}

RenderSystem::~RenderSystem() {
    // TODO: Device::wait_idle function.
    vkDeviceWaitIdle(*m_device);
}

std::uint32_t RenderSystem::upload_texture(const Texture &texture) {
    //    for (auto &frame_data : m_executable_graph->frame_datas()) {
    //        frame_data.transfer(m_texture_array, texture, m_texture_index);
    //    }
    return m_texture_index++;
}

void RenderSystem::update(World *world, float) {
    // Wait for previous frame rendering to finish, and request the next swapchain image.
    std::uint32_t image_index = m_swapchain.acquire_next_image(m_image_available_semaphores[m_frame_index], {});
    m_frame_fences[m_frame_index].block();
    m_frame_fences[m_frame_index].reset();

    m_back_buffer->set_image_index(image_index);
    m_executable_graph->start_frame(m_frame_index);
    {
        for (auto [entity, mesh, transform] : world->view<Mesh, Transform>()) {
            auto *mesh_offset = entity.get<MeshOffset>();
            auto matrix = mesh_offset != nullptr ? transform->translated(mesh_offset->offset()).scaled_matrix()
                                                 : transform->scaled_matrix();
            struct {
                glm::mat4 transform;
                glm::mat4 proj_view;
            } object_data{
                .transform = matrix,
                .proj_view = m_ubo.proj * m_ubo.view,
            };
            m_depth_pass->push_constants(object_data);
            m_main_pass->push_constants(object_data);
            m_depth_pass->draw_indexed(mesh->index_count(), mesh->index_offset());
            m_main_pass->draw_indexed(mesh->index_count(), mesh->index_offset());
        }
    }
    m_executable_graph->submit_frame(m_frame_fences[m_frame_index]);

    // Present output to swapchain.
    Array present_wait_semaphores{*m_rendering_finished_semaphores[m_frame_index]};
    m_swapchain.present(image_index, present_wait_semaphores);
    m_frame_index = (m_frame_index + 1) % m_executable_graph->frame_queue_length();
}
