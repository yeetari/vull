#include <vull/graphics/SkyboxRenderer.hh>

#include <vull/graphics/DefaultRenderer.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Result.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/vpak/Reader.hh>
#include <vull/vulkan/Buffer.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/DescriptorBuilder.hh>
#include <vull/vulkan/Image.hh>
#include <vull/vulkan/MemoryUsage.hh>
#include <vull/vulkan/Pipeline.hh>
#include <vull/vulkan/PipelineBuilder.hh>
#include <vull/vulkan/Queue.hh>
#include <vull/vulkan/RenderGraph.hh>
#include <vull/vulkan/Sampler.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull {

SkyboxRenderer::SkyboxRenderer(vk::Context &context, DefaultRenderer &default_renderer) : m_context(context) {
    vkb::DescriptorSetLayoutBinding binding{
        .binding = 0,
        .descriptorType = vkb::DescriptorType::CombinedImageSampler,
        .descriptorCount = 1,
        .stageFlags = vkb::ShaderStage::Fragment,
    };
    vkb::DescriptorSetLayoutCreateInfo set_layout_ci{
        .sType = vkb::StructureType::DescriptorSetLayoutCreateInfo,
        .flags = vkb::DescriptorSetLayoutCreateFlags::DescriptorBufferEXT,
        .bindingCount = 1,
        .pBindings = &binding,
    };
    VULL_ENSURE(m_context.vkCreateDescriptorSetLayout(&set_layout_ci, &m_set_layout) == vkb::Result::Success);

    m_pipeline = vk::PipelineBuilder()
                     // TODO(swapchain-format): Don't hardcode format.
                     .add_colour_attachment(vkb::Format::B8G8R8A8Srgb)
                     .add_set_layout(default_renderer.main_set_layout())
                     .add_set_layout(m_set_layout)
                     .add_shader(default_renderer.get_shader("skybox-vert"))
                     .add_shader(default_renderer.get_shader("skybox-frag"))
                     .set_depth_format(vkb::Format::D32Sfloat)
                     .set_depth_params(vkb::CompareOp::GreaterOrEqual, true, false)
                     .set_topology(vkb::PrimitiveTopology::TriangleList)
                     .build(m_context);

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

    const auto size = m_context.descriptor_size(vkb::DescriptorType::CombinedImageSampler);
    m_descriptor_buffer =
        m_context.create_buffer(size, vkb::BufferUsage::SamplerDescriptorBufferEXT | vkb::BufferUsage::TransferDst,
                                vk::MemoryUsage::DeviceOnly);
    auto staging_buffer = m_descriptor_buffer.create_staging();
    vk::DescriptorBuilder builder(m_set_layout, staging_buffer);
    builder.set(0, 0, m_image.full_view().sampled(vk::Sampler::Linear));
    m_descriptor_buffer.copy_from(staging_buffer, m_context.graphics_queue());
}

SkyboxRenderer::~SkyboxRenderer() {
    m_context.vkDestroyDescriptorSetLayout(m_set_layout);
}

vk::ResourceId SkyboxRenderer::build_pass(vk::RenderGraph &graph, vk::ResourceId target, vk::ResourceId depth_image,
                                          vk::ResourceId frame_ubo) {
    struct PassData {
        vk::ResourceId output;
        vk::ResourceId frame_ubo;
        vk::ResourceId depth_image;
    };
    return graph
        .add_pass<PassData>(
            "skybox", vk::PassFlags::Graphics,
            [&](vk::PassBuilder &builder, PassData &data) {
                data.output = builder.write(target, vk::WriteFlags::Additive);
                data.frame_ubo = builder.read(frame_ubo);
                data.depth_image = builder.read(depth_image);
            },
            [this](vk::RenderGraph &graph, vk::CommandBuffer &cmd_buf, const PassData &data) {
                cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Graphics, graph.get_buffer(data.frame_ubo), 0,
                                               0);
                cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Graphics, m_descriptor_buffer, 1, 0);
                cmd_buf.bind_pipeline(m_pipeline);
                cmd_buf.draw(36, 1);
            })
        .output;
}

void SkyboxRenderer::load(vpak::ReadStream &stream) {
    const auto pixel_count = 1024 * 1024 * 6;
    auto staging_buffer =
        m_context.create_buffer(pixel_count * 4, vkb::BufferUsage::TransferSrc, vk::MemoryUsage::HostOnly);
    auto *staging_data = staging_buffer.mapped<uint8_t>();
    for (uint32_t i = 0; i < pixel_count; i++) {
        VULL_EXPECT(stream.read({staging_data, 3}));
        staging_data += 4;
    }

    m_context.graphics_queue().immediate_submit([&](const vk::CommandBuffer &cmd_buf) {
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
        cmd_buf.image_barrier(transfer_write_barrier);
        cmd_buf.copy_buffer_to_image(staging_buffer, m_image, vkb::ImageLayout::TransferDstOptimal, copy);
        cmd_buf.image_barrier(image_read_barrier);
    });
}

} // namespace vull
