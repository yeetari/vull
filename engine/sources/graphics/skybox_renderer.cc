#include <vull/graphics/skybox_renderer.hh>

#include <vull/container/array.hh>
#include <vull/support/assert.hh>
#include <vull/support/function.hh>
#include <vull/support/result.hh>
#include <vull/support/span.hh>
#include <vull/support/stream.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/future.hh>
#include <vull/vulkan/buffer.hh>
#include <vull/vulkan/command_buffer.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/descriptor_builder.hh>
#include <vull/vulkan/image.hh>
#include <vull/vulkan/memory_usage.hh>
#include <vull/vulkan/pipeline.hh>
#include <vull/vulkan/pipeline_builder.hh>
#include <vull/vulkan/queue.hh>
#include <vull/vulkan/render_graph.hh>
#include <vull/vulkan/sampler.hh>
#include <vull/vulkan/shader.hh>
#include <vull/vulkan/vulkan.hh>

#include <stdint.h>

namespace vull {

SkyboxRenderer::SkyboxRenderer(vk::Context &context) : m_context(context) {
    Array set_bindings{
        // Frame UBO.
        vkb::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vkb::DescriptorType::UniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Vertex,
        },
        // Skybox image.
        vkb::DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = vkb::DescriptorType::CombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Fragment,
        },
    };
    vkb::DescriptorSetLayoutCreateInfo set_layout_ci{
        .sType = vkb::StructureType::DescriptorSetLayoutCreateInfo,
        .flags = vkb::DescriptorSetLayoutCreateFlags::DescriptorBufferEXT,
        .bindingCount = set_bindings.size(),
        .pBindings = set_bindings.data(),
    };
    VULL_ENSURE(m_context.vkCreateDescriptorSetLayout(&set_layout_ci, &m_set_layout) == vkb::Result::Success);
    m_context.vkGetDescriptorSetLayoutSizeEXT(m_set_layout, &m_set_layout_size);

    auto vertex_shader = VULL_EXPECT(vk::Shader::load(m_context, "/shaders/skybox.vert"));
    auto fragment_shader = VULL_EXPECT(vk::Shader::load(m_context, "/shaders/skybox.frag"));
    m_pipeline = VULL_EXPECT(vk::PipelineBuilder()
                                 // TODO(swapchain-format): Don't hardcode format.
                                 .add_colour_attachment(vkb::Format::B8G8R8A8Srgb)
                                 .add_set_layout(m_set_layout)
                                 .add_shader(vertex_shader)
                                 .add_shader(fragment_shader)
                                 .set_depth_format(vkb::Format::D32Sfloat)
                                 .set_depth_params(vkb::CompareOp::GreaterOrEqual, true, false)
                                 .set_topology(vkb::PrimitiveTopology::TriangleList)
                                 .build(m_context));

    vkb::ImageCreateInfo image_ci{
        .sType = vkb::StructureType::ImageCreateInfo,
        .flags = vkb::ImageCreateFlags::CubeCompatible,
        .imageType = vkb::ImageType::_2D,
        .format = vkb::Format::R8G8B8A8Srgb,
        .extent = {1024, 1024, 1},
        .mipLevels = 1,
        .arrayLayers = 6,
        .samples = vkb::SampleCount::_1,
        .tiling = vkb::ImageTiling::Optimal,
        .usage = vkb::ImageUsage::Sampled | vkb::ImageUsage::TransferDst,
        .sharingMode = vkb::SharingMode::Exclusive,
        .initialLayout = vkb::ImageLayout::Undefined,
    };
    m_image = m_context.create_image(image_ci, vk::MemoryUsage::DeviceOnly);
}

SkyboxRenderer::~SkyboxRenderer() {
    m_context.vkDestroyDescriptorSetLayout(m_set_layout);
}

void SkyboxRenderer::build_pass(vk::RenderGraph &graph, vk::ResourceId &depth_image, vk::ResourceId &frame_ubo,
                                vk::ResourceId &target) {
    vk::BufferDescription descriptor_buffer_description{
        .size = m_set_layout_size,
        .usage = vkb::BufferUsage::SamplerDescriptorBufferEXT | vkb::BufferUsage::ResourceDescriptorBufferEXT,
        .host_accessible = true,
    };
    auto descriptor_buffer_id = graph.new_buffer("skybox-descriptor-buffer", descriptor_buffer_description);

    auto &pass = graph.add_pass("skybox", vk::PassFlag::Graphics)
                     .write(target, vk::WriteFlag::Additive)
                     .read(depth_image)
                     .read(frame_ubo);

    pass.set_on_execute([=, this, &graph](vk::CommandBuffer &cmd_buf) {
        const auto &descriptor_buffer = graph.get_buffer(descriptor_buffer_id);
        vk::DescriptorBuilder descriptor_builder(m_set_layout, descriptor_buffer);
        descriptor_builder.set(0, 0, graph.get_buffer(frame_ubo));
        descriptor_builder.set(1, 0, m_image.full_view().sampled(vk::Sampler::Linear));

        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Graphics, descriptor_buffer, 0, 0);
        cmd_buf.bind_pipeline(m_pipeline);
        cmd_buf.draw(36, 1);
    });
}

void SkyboxRenderer::load(Stream &stream) {
    const auto pixel_count = 1024 * 1024 * 6;
    auto staging_buffer = m_context.create_buffer(static_cast<vkb::DeviceSize>(pixel_count) * 4,
                                                  vkb::BufferUsage::TransferSrc, vk::MemoryUsage::HostOnly);
    auto *staging_data = staging_buffer.mapped<uint8_t>();
    for (uint32_t i = 0; i < pixel_count; i++) {
        VULL_EXPECT(stream.read({staging_data, 3}));
        staging_data += 4;
    }

    // TODO: Don't immediately await the submit.
    auto &queue = m_context.get_queue(vk::QueueKind::Transfer);
    auto cmd_buf = queue.request_cmd_buf();
    vkb::ImageMemoryBarrier2 transfer_write_barrier{
        .sType = vkb::StructureType::ImageMemoryBarrier2,
        .dstStageMask = vkb::PipelineStage2::Copy,
        .dstAccessMask = vkb::Access2::TransferWrite,
        .oldLayout = vkb::ImageLayout::Undefined,
        .newLayout = vkb::ImageLayout::TransferDstOptimal,
        .image = *m_image,
        .subresourceRange = m_image.full_view().range(),
    };
    vkb::BufferImageCopy copy{
        .imageSubresource{
            .aspectMask = vkb::ImageAspect::Color,
            .layerCount = 6,
        },
        .imageExtent = {1024, 1024, 1},
    };
    vkb::ImageMemoryBarrier2 image_read_barrier{
        .sType = vkb::StructureType::ImageMemoryBarrier2,
        .srcStageMask = vkb::PipelineStage2::Copy,
        .srcAccessMask = vkb::Access2::TransferWrite,
        .dstStageMask = vkb::PipelineStage2::AllCommands,
        .dstAccessMask = vkb::Access2::ShaderRead,
        .oldLayout = vkb::ImageLayout::TransferDstOptimal,
        .newLayout = vkb::ImageLayout::ReadOnlyOptimal,
        .image = *m_image,
        .subresourceRange = m_image.full_view().range(),
    };
    cmd_buf->image_barrier(transfer_write_barrier);
    cmd_buf->copy_buffer_to_image(staging_buffer, m_image, vkb::ImageLayout::TransferDstOptimal, copy);
    cmd_buf->image_barrier(image_read_barrier);
    queue.submit(vull::move(cmd_buf), {}, {}).await();
}

} // namespace vull
