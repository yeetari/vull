#include <vull/renderer/RenderSystem.hh>

#include <vull/core/Entity.hh>
#include <vull/core/Transform.hh>
#include <vull/core/World.hh>
#include <vull/io/FileSystem.hh>
#include <vull/renderer/Material.hh>
#include <vull/renderer/Mesh.hh>
#include <vull/renderer/MeshOffset.hh>
#include <vull/renderer/PointLight.hh>
#include <vull/renderer/RenderGraph.hh>
#include <vull/renderer/UniformBuffer.hh>
#include <vull/renderer/Vertex.hh>
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

    create_queue();
    create_sync_objects();

    Shader depth_pass_shader(device, FileSystem::load_shader("builtin/shaders/depth.vert"));
    Shader light_cull_pass_shader(device, FileSystem::load_shader("builtin/shaders/light_cull.comp"));
    Shader main_pass_vertex_shader(device, FileSystem::load_shader("builtin/shaders/main.vert"));
    Shader main_pass_fragment_shader(device, FileSystem::load_shader("builtin/shaders/main.frag"));

    auto *back_buffer = m_graph.add<SwapchainResource>(swapchain);
    back_buffer->set_clear_value(VkClearValue{.color{{0.4f, 0.6f, 0.7f, 1.0f}}});

    auto *depth_buffer = m_graph.add<ImageResource>(ImageType::Depth, MemoryUsage::GpuOnly);
    depth_buffer->set_clear_value(VkClearValue{.depthStencil{1.0f, 0}});
    depth_buffer->set_extent(swapchain.extent());
    depth_buffer->set_format(VK_FORMAT_D32_SFLOAT);

    m_texture_array = m_graph.add<ImageResource>(ImageType::Array, MemoryUsage::TransferOnce);
    m_texture_array->set_format(VK_FORMAT_BC2_SRGB_BLOCK);
    m_texture_array->set_image_count(500);
    m_texture_array->set_name("texture array");

    auto *index_buffer = m_graph.add<BufferResource>(BufferType::IndexBuffer, MemoryUsage::TransferOnce);
    auto *vertex_buffer = m_graph.add<BufferResource>(BufferType::VertexBuffer, MemoryUsage::TransferOnce);
    vertex_buffer->add_vertex_attribute(VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position));
    vertex_buffer->add_vertex_attribute(VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal));
    vertex_buffer->add_vertex_attribute(VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv));
    vertex_buffer->set_element_size(sizeof(Vertex));

    auto *light_visibility_buffer = m_graph.add<BufferResource>(BufferType::StorageBuffer, MemoryUsage::GpuOnly);
    light_visibility_buffer->set_initial_size(k_light_visibility_size * m_row_tile_count * m_col_tile_count);
    light_visibility_buffer->set_name("light visibility buffer");

    m_light_buffer = m_graph.add<BufferResource>(BufferType::StorageBuffer, MemoryUsage::HostVisible);
    m_uniform_buffer = m_graph.add<BufferResource>(BufferType::UniformBuffer, MemoryUsage::HostVisible);
    m_light_buffer->set_initial_size(k_lights_buffer_size);
    m_light_buffer->set_name("light buffer");

    m_depth_pass = m_graph.add<GraphicsStage>("depth pass");
    m_depth_pass->add_shader(depth_pass_shader);
    m_depth_pass->reads_from(index_buffer);
    m_depth_pass->reads_from(m_uniform_buffer);
    m_depth_pass->reads_from(vertex_buffer);
    m_depth_pass->add_output(depth_buffer);

    auto *light_cull_pass = m_graph.add<ComputeStage>("light cull pass");
    light_cull_pass->add_shader(light_cull_pass_shader);
    light_cull_pass->reads_from(depth_buffer);
    light_cull_pass->reads_from(m_light_buffer);
    light_cull_pass->reads_from(m_uniform_buffer);
    light_cull_pass->set_constant("k_tile_size", k_tile_size);
    light_cull_pass->set_constant("k_max_lights_per_tile", k_max_lights_per_tile);
    light_cull_pass->set_constant("k_row_tile_count", m_row_tile_count);
    light_cull_pass->set_constant("k_viewport_width", swapchain_extent.width);
    light_cull_pass->set_constant("k_viewport_height", swapchain_extent.height);
    light_cull_pass->writes_to(light_visibility_buffer);

    m_main_pass = m_graph.add<GraphicsStage>("main pass");
    m_main_pass->add_shader(main_pass_vertex_shader);
    m_main_pass->add_shader(main_pass_fragment_shader);
    m_main_pass->reads_from(index_buffer);
    m_main_pass->reads_from(m_light_buffer);
    m_main_pass->reads_from(light_visibility_buffer);
    m_main_pass->reads_from(m_uniform_buffer);
    m_main_pass->reads_from(vertex_buffer);
    m_main_pass->reads_from(m_texture_array);
    m_main_pass->set_constant("k_tile_size", k_tile_size);
    m_main_pass->set_constant("k_max_lights_per_tile", k_max_lights_per_tile);
    m_main_pass->set_constant("k_row_tile_count", m_row_tile_count);
    m_main_pass->add_input(depth_buffer);
    m_main_pass->add_output(back_buffer);

    light_cull_pass->set_on_record([this](VkCommandBuffer cmd_buf, VkPipelineLayout) {
        vkCmdDispatch(cmd_buf, m_row_tile_count, m_col_tile_count, 1);
    });

    m_compiled_graph = m_graph.compile(back_buffer);
    m_executable_graph = m_compiled_graph->build_objects(device, swapchain.image_count());

    for (std::uint32_t i = 0; i < m_executable_graph->frame_datas().size(); i++) {
        auto &frame_data = m_executable_graph->frame_data(i);
        frame_data.insert_signal_semaphore(m_main_pass, m_rendering_finished_semaphores[i]);
        frame_data.insert_wait_semaphore(m_main_pass, m_image_available_semaphores[i],
                                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        frame_data.transfer(index_buffer, indices);
        frame_data.transfer(vertex_buffer, vertices);
    }
}

RenderSystem::~RenderSystem() {
    vkDeviceWaitIdle(*m_device);
}

void RenderSystem::create_queue() {
    for (std::uint32_t i = 0; const auto &queue_family : m_device.queue_families()) {
        const auto flags = queue_family.queueFlags;
        if ((flags & VK_QUEUE_COMPUTE_BIT) != 0u && (flags & VK_QUEUE_GRAPHICS_BIT) != 0u) {
            const std::uint32_t queue_index = 0;
            Log::trace("renderer", "Using queue %d:%d", i, queue_index);
            vkGetDeviceQueue(*m_device, i, queue_index, &m_queue);
        }
    }
    // This will still be nullptr if no queue supporting both compute and graphics is found.
    ENSURE(m_queue != nullptr);
}

void RenderSystem::create_sync_objects() {
    m_frame_fences.resize(m_swapchain.image_count(), m_device, true);
    m_image_available_semaphores.resize(m_swapchain.image_count(), m_device);
    m_rendering_finished_semaphores.resize(m_swapchain.image_count(), m_device);
}

std::uint32_t RenderSystem::upload_texture(const Texture &texture) {
    for (auto &frame_data : m_executable_graph->frame_datas()) {
        frame_data.transfer(m_texture_array, texture, m_texture_index);
    }
    return m_texture_index++;
}

void RenderSystem::update(World *world, float) {
    m_depth_pass->set_on_record([world](VkCommandBuffer cmd_buf, VkPipelineLayout pipeline_layout) {
        for (auto [entity, mesh, transform] : world->view<Mesh, Transform>()) {
            auto *mesh_offset = entity.get<MeshOffset>();
            auto matrix = mesh_offset != nullptr ? transform->translated(mesh_offset->offset()).scaled_matrix()
                                                 : transform->scaled_matrix();
            vkCmdPushConstants(cmd_buf, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &matrix);
            vkCmdDrawIndexed(cmd_buf, mesh->index_count(), 1, mesh->index_offset(), 0, 0);
        }
    });
    m_main_pass->set_on_record([world](VkCommandBuffer cmd_buf, VkPipelineLayout pipeline_layout) {
        for (auto [entity, material, mesh, transform] : world->view<Material, Mesh, Transform>()) {
            auto *mesh_offset = entity.get<MeshOffset>();
            auto matrix = mesh_offset != nullptr ? transform->translated(mesh_offset->offset()).scaled_matrix()
                                                 : transform->scaled_matrix();
            struct PushConstant {
                glm::mat4 transform;
                std::uint32_t albedo_index;
            } push_constant{
                .transform = matrix,
                .albedo_index = material->albedo_index(),
            };
            vkCmdPushConstants(cmd_buf, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(PushConstant), &push_constant);
            vkCmdDrawIndexed(cmd_buf, mesh->index_count(), 1, mesh->index_offset(), 0, 0);
        }
    });

    // Wait for previous frame rendering to finish, and request the next swapchain image.
    std::uint32_t image_index = m_swapchain.acquire_next_image(m_image_available_semaphores[m_frame_index], {});
    m_frame_fences[m_frame_index].block();
    m_frame_fences[m_frame_index].reset();

    auto &frame_data = m_executable_graph->frame_data(m_frame_index);
    frame_data.upload(m_uniform_buffer, m_ubo);

    // Update dynamic light data.
    const std::uint32_t light_count = m_lights.size();
    frame_data.upload(m_light_buffer, light_count);
    frame_data.upload(m_light_buffer, m_lights, sizeof(glm::vec4));

    Array swapchain_indices{image_index};
    m_executable_graph->render(m_frame_index, m_queue, m_frame_fences[m_frame_index], swapchain_indices);

    // Present output to swapchain.
    Array present_wait_semaphores{*m_rendering_finished_semaphores[m_frame_index]};
    m_swapchain.present(image_index, present_wait_semaphores);
    m_frame_index = (m_frame_index + 1) % m_swapchain.image_count();
}
