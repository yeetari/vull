#include <vull/graphics/DeferredRenderer.hh>

#include <vull/container/Array.hh>
#include <vull/container/Vector.hh>
#include <vull/graphics/GBuffer.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Result.hh>
#include <vull/support/Utility.hh>
#include <vull/vulkan/Buffer.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/DescriptorBuilder.hh>
#include <vull/vulkan/Image.hh>
#include <vull/vulkan/Pipeline.hh>
#include <vull/vulkan/PipelineBuilder.hh>
#include <vull/vulkan/RenderGraph.hh>
#include <vull/vulkan/Sampler.hh>
#include <vull/vulkan/Shader.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>
#include <string.h>

namespace vull {
namespace {

constexpr uint32_t k_tile_size = 32;
constexpr uint32_t k_tile_max_light_count = 256;

struct PointLight {
    Vec3f position;
    float radius{0.0f};
    Vec3f colour;
    float padding{0.0f};
};

} // namespace

DeferredRenderer::DeferredRenderer(vk::Context &context, vkb::Extent3D viewport_extent)
    : m_context(context), m_viewport_extent(viewport_extent) {
    m_tile_extent = {
        .width = vull::ceil_div(viewport_extent.width, k_tile_size),
        .height = vull::ceil_div(viewport_extent.height, k_tile_size),
    };
    create_set_layouts();
    create_pipelines();
}

DeferredRenderer::~DeferredRenderer() {
    m_context.vkDestroyDescriptorSetLayout(m_set_layout);
}

void DeferredRenderer::create_set_layouts() {
    Array set_bindings{
        // Frame UBO.
        vkb::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vkb::DescriptorType::UniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        // Light buffer.
        vkb::DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        // Light visibility buffer.
        vkb::DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        // Albedo image.
        vkb::DescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = vkb::DescriptorType::SampledImage,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        // Normal image.
        vkb::DescriptorSetLayoutBinding{
            .binding = 4,
            .descriptorType = vkb::DescriptorType::SampledImage,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        // Depth image.
        vkb::DescriptorSetLayoutBinding{
            .binding = 5,
            .descriptorType = vkb::DescriptorType::SampledImage,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        // HDR image.
        vkb::DescriptorSetLayoutBinding{
            .binding = 6,
            .descriptorType = vkb::DescriptorType::StorageImage,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        // HDR image (sampled).
        vkb::DescriptorSetLayoutBinding{
            .binding = 7,
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
}

void DeferredRenderer::create_pipelines() {
    auto light_cull_shader = VULL_EXPECT(vk::Shader::load(m_context, "/shaders/light_cull.comp"));
    m_light_cull_pipeline = VULL_EXPECT(vk::PipelineBuilder()
                                            .add_set_layout(m_set_layout)
                                            .add_shader(light_cull_shader)
                                            .set_constant("k_viewport_width", m_viewport_extent.width)
                                            .set_constant("k_viewport_height", m_viewport_extent.height)
                                            .set_constant("k_tile_size", k_tile_size)
                                            .set_constant("k_tile_max_light_count", k_tile_max_light_count)
                                            .build(m_context));

    auto deferred_shader = VULL_EXPECT(vk::Shader::load(m_context, "/shaders/deferred.comp"));
    m_deferred_pipeline = VULL_EXPECT(vk::PipelineBuilder()
                                          .add_set_layout(m_set_layout)
                                          .add_shader(deferred_shader)
                                          .set_constant("k_viewport_width", m_viewport_extent.width)
                                          .set_constant("k_viewport_height", m_viewport_extent.height)
                                          .set_constant("k_tile_size", k_tile_size)
                                          .set_constant("k_row_tile_count", m_tile_extent.width)
                                          .build(m_context));

    auto triangle_shader = VULL_EXPECT(vk::Shader::load(m_context, "/shaders/fst.vert"));
    auto blit_tonemap_shader = VULL_EXPECT(vk::Shader::load(m_context, "/shaders/blit_tonemap.frag"));
    m_blit_tonemap_pipeline = VULL_EXPECT(vk::PipelineBuilder()
                                              // TODO(swapchain-format): Don't hardcode format.
                                              .add_colour_attachment(vkb::Format::B8G8R8A8Srgb)
                                              .add_set_layout(m_set_layout)
                                              .add_shader(triangle_shader)
                                              .add_shader(blit_tonemap_shader)
                                              .set_topology(vkb::PrimitiveTopology::TriangleList)
                                              .build(m_context));
}

GBuffer DeferredRenderer::create_gbuffer(vk::RenderGraph &graph) {
    vk::AttachmentDescription albedo_description{
        .extent = {m_viewport_extent.width, m_viewport_extent.height},
        .format = vkb::Format::R8G8B8A8Unorm,
        .usage = vkb::ImageUsage::ColorAttachment | vkb::ImageUsage::Sampled,
    };
    vk::AttachmentDescription normal_description{
        .extent = {m_viewport_extent.width, m_viewport_extent.height},
        .format = vkb::Format::R16G16Snorm,
        .usage = vkb::ImageUsage::ColorAttachment | vkb::ImageUsage::Sampled,
    };
    vk::AttachmentDescription depth_description{
        .extent = {m_viewport_extent.width, m_viewport_extent.height},
        .format = vkb::Format::D32Sfloat,
        .usage = vkb::ImageUsage::DepthStencilAttachment | vkb::ImageUsage::Sampled,
    };
    return GBuffer{
        .albedo = graph.new_attachment("gbuffer-albedo", albedo_description),
        .normal = graph.new_attachment("gbuffer-normal", normal_description),
        .depth = graph.new_attachment("gbuffer-depth", depth_description),
    };
}

void DeferredRenderer::build_pass(vk::RenderGraph &graph, GBuffer &gbuffer, vk::ResourceId &frame_ubo,
                                  vk::ResourceId &target) {
    Vector<PointLight> lights;
    lights.push(PointLight{
        .position = Vec3f(0.0f),
        .radius = 1.0f,
        .colour = Vec3f(1.0f),
    });

    vk::BufferDescription descriptor_buffer_description{
        .size = m_set_layout_size,
        .usage = vkb::BufferUsage::SamplerDescriptorBufferEXT | vkb::BufferUsage::ResourceDescriptorBufferEXT,
        .host_accessible = true,
    };
    vk::BufferDescription light_buffer_description{
        .size = lights.size_bytes() + sizeof(float),
        .usage = vkb::BufferUsage::StorageBuffer,
        .host_accessible = true,
    };
    vk::BufferDescription visibility_buffer_description{
        .size =
            (sizeof(uint32_t) + k_tile_max_light_count * sizeof(uint32_t)) * m_tile_extent.width * m_tile_extent.height,
        .usage = vkb::BufferUsage::StorageBuffer,
    };
    auto descriptor_buffer_id = graph.new_buffer("deferred-descriptor-buffer", descriptor_buffer_description);
    auto light_buffer_id = graph.new_buffer("light-buffer", light_buffer_description);
    auto visibility_buffer_id = graph.new_buffer("light-visibility", visibility_buffer_description);

    auto &light_cull_pass = graph.add_pass("light-cull", vk::PassFlags::Compute)
                                .read(gbuffer.depth)
                                .write(descriptor_buffer_id)
                                .write(light_buffer_id)
                                .write(visibility_buffer_id);
    light_cull_pass.set_on_execute([=, this, &graph, lights = vull::move(lights)](vk::CommandBuffer &cmd_buf) {
        const auto &descriptor_buffer = graph.get_buffer(descriptor_buffer_id);
        const auto &light_buffer = graph.get_buffer(light_buffer_id);
        vk::DescriptorBuilder descriptor_builder(m_set_layout, descriptor_buffer);
        descriptor_builder.set(0, 0, graph.get_buffer(frame_ubo));
        descriptor_builder.set(1, 0, light_buffer);
        descriptor_builder.set(2, 0, graph.get_buffer(visibility_buffer_id));

        const auto &albedo_image = graph.get_image(gbuffer.albedo);
        const auto &normal_image = graph.get_image(gbuffer.normal);
        const auto &depth_image = graph.get_image(gbuffer.depth);
        descriptor_builder.set(3, 0, albedo_image.full_view().sampled(vk::Sampler::None));
        descriptor_builder.set(4, 0, normal_image.full_view().sampled(vk::Sampler::None));
        descriptor_builder.set(5, 0, depth_image.full_view().sampled(vk::Sampler::None));

        uint32_t light_count = lights.size();
        memcpy(light_buffer.mapped_raw(), &light_count, sizeof(uint32_t));
        memcpy(light_buffer.mapped<float>() + 4, lights.data(), lights.size_bytes());

        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Compute, descriptor_buffer, 0, 0);
        cmd_buf.bind_pipeline(m_light_cull_pipeline);
        cmd_buf.dispatch(m_tile_extent.width, m_tile_extent.height);
    });

    vk::AttachmentDescription hdr_image_description{
        .extent = {m_viewport_extent.width, m_viewport_extent.height},
        .format = vkb::Format::R16G16B16A16Sfloat,
        .usage = vkb::ImageUsage::Storage | vkb::ImageUsage::TransferSrc,
    };
    auto hdr_image_id = graph.new_attachment("hdr-image", hdr_image_description);
    auto &deferred_pass = graph.add_pass("deferred", vk::PassFlags::Compute)
                              .read(gbuffer.albedo)
                              .read(gbuffer.normal)
                              .read(gbuffer.depth)
                              .read(visibility_buffer_id)
                              .write(hdr_image_id);
    deferred_pass.set_on_execute([=, this, &graph](vk::CommandBuffer &cmd_buf) {
        const auto &descriptor_buffer = graph.get_buffer(descriptor_buffer_id);
        vk::DescriptorBuilder descriptor_builder(m_set_layout, descriptor_buffer);
        descriptor_builder.set(6, 0, graph.get_image(hdr_image_id).full_view());

        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Compute, descriptor_buffer, 0, 0);
        cmd_buf.bind_pipeline(m_deferred_pipeline);
        cmd_buf.dispatch(vull::ceil_div(m_viewport_extent.width, 8u), vull::ceil_div(m_viewport_extent.height, 8u));
    });

    auto &blit_tonemap_pass = graph.add_pass("blit-tonemap", vk::PassFlags::Graphics)
                                  .read(hdr_image_id, vk::ReadFlags::Sampled)
                                  .write(target);
    blit_tonemap_pass.set_on_execute([=, this, &graph](vk::CommandBuffer &cmd_buf) {
        const auto &descriptor_buffer = graph.get_buffer(descriptor_buffer_id);
        vk::DescriptorBuilder descriptor_builder(m_set_layout, descriptor_buffer);
        descriptor_builder.set(7, 0, graph.get_image(hdr_image_id).full_view().sampled(vk::Sampler::Nearest));

        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Graphics, descriptor_buffer, 0, 0);
        cmd_buf.bind_pipeline(m_blit_tonemap_pipeline);
        cmd_buf.draw(3, 1);
    });
}

} // namespace vull
