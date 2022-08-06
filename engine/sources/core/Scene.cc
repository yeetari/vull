#include <vull/core/Scene.hh>

#include <vull/core/BoundingBox.hh>
#include <vull/core/Log.hh>
#include <vull/core/Material.hh>
#include <vull/core/Mesh.hh>
#include <vull/core/Transform.hh>
#include <vull/ecs/Entity.hh>
#include <vull/ecs/EntityId.hh>
#include <vull/ecs/World.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Mat.hh>
#include <vull/support/Assert.hh>
#include <vull/support/HashMap.hh>
#include <vull/support/HashSet.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Span.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Tuple.hh>
#include <vull/support/Vector.hh>
#include <vull/vpak/PackFile.hh>
#include <vull/vpak/Reader.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Queue.hh>
#include <vull/vulkan/Vulkan.hh>

// IWYU pragma: no_forward_declare vull::vkb::Image_T

namespace vull {
namespace {

constexpr auto k_staging_buffer_size = 1024ul * 1024ul * 16ul;

struct FormatInfo {
    vkb::Format format;
    uint32_t unit_size;
    bool block_compressed;
};

FormatInfo parse_format(uint8_t pack_format) {
    switch (vpak::ImageFormat(pack_format)) {
    case vpak::ImageFormat::Bc1Srgb:
        return {vkb::Format::Bc1RgbSrgbBlock, 8u, true};
    case vpak::ImageFormat::Bc3Srgba:
        return {vkb::Format::Bc3SrgbBlock, 16u, true};
    case vpak::ImageFormat::Bc5Unorm:
        return {vkb::Format::Bc5UnormBlock, 16u, true};
    case vpak::ImageFormat::RgUnorm:
        return {vkb::Format::R8G8Unorm, 2u, false};
    case vpak::ImageFormat::RgbaUnorm:
        return {vkb::Format::R8G8B8A8Unorm, 4u, false};
    default:
        return {vkb::Format::Undefined, 0u, false};
    }
}

} // namespace

Scene::~Scene() {
    for (auto *texture_view : m_texture_views) {
        m_context.vkDestroyImageView(texture_view);
    }
    for (auto *texture_image : m_texture_images) {
        m_context.vkDestroyImage(texture_image);
    }
    for (const auto &[name, index_buffer] : m_index_buffers) {
        m_context.vkDestroyBuffer(index_buffer);
    }
    for (const auto &[name, vertex_buffer] : m_vertex_buffers) {
        m_context.vkDestroyBuffer(vertex_buffer);
    }
}

Mat4f Scene::get_transform_matrix(EntityId entity) {
    const auto &transform = m_world.get_component<Transform>(entity);
    if (transform.parent() == ~vull::EntityId(0)) {
        // Root node.
        return transform.matrix();
    }
    const auto parent_matrix = get_transform_matrix(transform.parent());
    return parent_matrix * transform.matrix();
}

vkb::Buffer Scene::load_buffer(vk::CommandPool &cmd_pool, vk::Queue &queue, vpak::ReadStream &stream,
                               vkb::Buffer staging_buffer, void *staging_data, vkb::DeviceSize &memory_offset,
                               uint32_t size, vkb::BufferUsage usage) {
    vkb::BufferCreateInfo buffer_ci{
        .sType = vkb::StructureType::BufferCreateInfo,
        .size = size,
        .usage = usage | vkb::BufferUsage::TransferDst,
        .sharingMode = vkb::SharingMode::Exclusive,
    };
    vkb::Buffer buffer;
    VULL_ENSURE(m_context.vkCreateBuffer(&buffer_ci, &buffer) == vkb::Result::Success);

    vkb::MemoryRequirements memory_requirements{};
    m_context.vkGetBufferMemoryRequirements(buffer, &memory_requirements);
    memory_offset = (memory_offset + memory_requirements.alignment - 1) & ~(memory_requirements.alignment - 1);
    VULL_ENSURE(m_context.vkBindBufferMemory(buffer, m_memory, memory_offset) == vkb::Result::Success);

    VULL_ENSURE(size <= k_staging_buffer_size);
    stream.read({staging_data, size});

    queue.immediate_submit(cmd_pool, [=](const vk::CommandBuffer &cmd_buf) {
        vkb::BufferCopy copy{
            .size = size,
        };
        cmd_buf.copy_buffer(staging_buffer, buffer, copy);
    });
    memory_offset += size;
    return buffer;
}

void Scene::load_image(vk::CommandPool &cmd_pool, vk::Queue &queue, vpak::ReadStream &stream,
                       vkb::Buffer staging_buffer, void *staging_data, vkb::DeviceSize &memory_offset) {
    const auto [format, unit_size, block_compressed] = parse_format(stream.read_byte());
    // TODO(stream-api): templated read_varint.
    const auto width = static_cast<uint32_t>(stream.read_varint());
    const auto height = static_cast<uint32_t>(stream.read_varint());
    const auto mip_count = static_cast<uint32_t>(stream.read_varint());

    // TODO: What's the best thing to do if this happens?
    uint32_t expected_mip_count = 32u - vull::clz(vull::max(width, height));
    if (mip_count != expected_mip_count) {
        vull::warn("[scene] Expected {} mips, but got {}", expected_mip_count, mip_count);
    }

    vkb::ImageCreateInfo image_ci{
        .sType = vkb::StructureType::ImageCreateInfo,
        .imageType = vkb::ImageType::_2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = mip_count,
        .arrayLayers = 1,
        .samples = vkb::SampleCount::_1,
        .tiling = vkb::ImageTiling::Optimal,
        .usage = vkb::ImageUsage::TransferDst | vkb::ImageUsage::Sampled,
        .sharingMode = vkb::SharingMode::Exclusive,
        .initialLayout = vkb::ImageLayout::Undefined,
    };
    auto &image = m_texture_images.emplace();
    VULL_ENSURE(m_context.vkCreateImage(&image_ci, &image) == vkb::Result::Success);

    vkb::MemoryRequirements memory_requirements{};
    m_context.vkGetImageMemoryRequirements(image, &memory_requirements);
    memory_offset = (memory_offset + memory_requirements.alignment - 1) & ~(memory_requirements.alignment - 1);
    VULL_ENSURE(m_context.vkBindImageMemory(image, m_memory, memory_offset) == vkb::Result::Success);

    vkb::ImageViewCreateInfo image_view_ci{
        .sType = vkb::StructureType::ImageViewCreateInfo,
        .image = image,
        .viewType = vkb::ImageViewType::_2D,
        .format = format,
        .subresourceRange{
            .aspectMask = vkb::ImageAspect::Color,
            .levelCount = mip_count,
            .layerCount = 1,
        },
    };
    VULL_ENSURE(m_context.vkCreateImageView(&image_view_ci, &m_texture_views.emplace()) == vkb::Result::Success);

    // Transition the whole image (all mip levels) to TransferDstOptimal.
    queue.immediate_submit(cmd_pool, [image, mip_count](const vk::CommandBuffer &cmd_buf) {
        vkb::ImageMemoryBarrier2 transfer_write_barrier{
            .sType = vkb::StructureType::ImageMemoryBarrier2,
            .dstStageMask = vkb::PipelineStage2::Copy,
            .dstAccessMask = vkb::Access2::TransferWrite,
            .oldLayout = vkb::ImageLayout::Undefined,
            .newLayout = vkb::ImageLayout::TransferDstOptimal,
            .image = image,
            .subresourceRange{
                .aspectMask = vkb::ImageAspect::Color,
                .levelCount = mip_count,
                .layerCount = 1,
            },
        };
        cmd_buf.image_barrier(transfer_write_barrier);
    });

    uint32_t mip_width = width;
    uint32_t mip_height = height;
    for (uint32_t i = 0; i < mip_count; i++) {
        const uint32_t mip_size = block_compressed ? ((mip_width + 3) / 4) * ((mip_height + 3) / 4) * unit_size
                                                   : mip_width * mip_height * unit_size;
        VULL_ENSURE(mip_size <= k_staging_buffer_size);
        stream.read({staging_data, mip_size});

        // Perform CPU -> GPU copy.
        queue.immediate_submit(cmd_pool, [=](const vk::CommandBuffer &cmd_buf) {
            vkb::BufferImageCopy copy{
                .imageSubresource{
                    .aspectMask = vkb::ImageAspect::Color,
                    .mipLevel = i,
                    .layerCount = 1,
                },
                .imageExtent = {mip_width, mip_height, 1},
            };
            cmd_buf.copy_buffer_to_image(staging_buffer, image, vkb::ImageLayout::TransferDstOptimal, copy);
        });
        mip_width >>= 1;
        mip_height >>= 1;
        memory_offset += mip_size;
    }

    // Transition the whole image to ShaderReadOnlyOptimal.
    queue.immediate_submit(cmd_pool, [image, mip_count](const vk::CommandBuffer &cmd_buf) {
        vkb::ImageMemoryBarrier2 image_read_barrier{
            .sType = vkb::StructureType::ImageMemoryBarrier2,
            .srcStageMask = vkb::PipelineStage2::Copy,
            .srcAccessMask = vkb::Access2::TransferWrite,
            .dstStageMask = vkb::PipelineStage2::AllCommands,
            .dstAccessMask = vkb::Access2::ShaderRead,
            .oldLayout = vkb::ImageLayout::TransferDstOptimal,
            .newLayout = vkb::ImageLayout::ShaderReadOnlyOptimal,
            .image = image,
            .subresourceRange{
                .aspectMask = vkb::ImageAspect::Color,
                .levelCount = mip_count,
                .layerCount = 1,
            },
        };
        cmd_buf.image_barrier(image_read_barrier);
    });
}

void Scene::load(vk::CommandPool &cmd_pool, vk::Queue &queue, StringView path) {
    // For now, allocate a fixed amount of VRAM to store all the scene's resources in.
    vkb::MemoryRequirements memory_requirements{
        .size = 1024ul * 1024ul * 2048ul,
        .memoryTypeBits = 0xffffffffu,
    };
    m_memory = m_context.allocate_memory(memory_requirements, vk::MemoryType::DeviceLocal);

    // Read pack header and register default components. Note that the order currently matters.
    vpak::Reader pack_reader(path);
    m_world.register_component<Transform>();
    m_world.register_component<Mesh>();
    m_world.register_component<Material>();
    m_world.register_component<BoundingBox>();

    // Allocate a temporary staging buffer used to upload data to VRAM.
    vkb::BufferCreateInfo staging_buffer_ci{
        .sType = vkb::StructureType::BufferCreateInfo,
        .size = k_staging_buffer_size,
        .usage = vkb::BufferUsage::TransferSrc,
        .sharingMode = vkb::SharingMode::Exclusive,
    };
    vkb::Buffer staging_buffer;
    VULL_ENSURE(m_context.vkCreateBuffer(&staging_buffer_ci, &staging_buffer) == vkb::Result::Success);
    vkb::MemoryRequirements staging_memory_requirements{};
    m_context.vkGetBufferMemoryRequirements(staging_buffer, &staging_memory_requirements);
    auto *staging_memory = m_context.allocate_memory(staging_memory_requirements, vk::MemoryType::Staging);
    VULL_ENSURE(m_context.vkBindBufferMemory(staging_buffer, staging_memory, 0) == vkb::Result::Success);
    void *staging_data;
    VULL_ENSURE(m_context.vkMapMemory(staging_memory, 0, vkb::k_whole_size, 0, &staging_data) == vkb::Result::Success);

    // Load world.
    m_world.deserialise(pack_reader);

    // Preload all meshes.
    vkb::DeviceSize memory_offset = 0;
    for (auto [entity, mesh] : m_world.view<Mesh>()) {
        if (auto name = mesh.vertex_data_name(); !m_vertex_buffers.contains(name)) {
            auto entry = *pack_reader.stat(name);
            auto stream = *pack_reader.open(name);
            auto *buffer = load_buffer(cmd_pool, queue, stream, staging_buffer, staging_data, memory_offset, entry.size,
                                       vkb::BufferUsage::VertexBuffer);
            m_vertex_buffers.set(name, buffer);
        }
        if (auto name = mesh.index_data_name(); !m_index_buffers.contains(name)) {
            auto entry = *pack_reader.stat(name);
            auto stream = *pack_reader.open(name);
            auto *buffer = load_buffer(cmd_pool, queue, stream, staging_buffer, staging_data, memory_offset, entry.size,
                                       vkb::BufferUsage::IndexBuffer);
            m_index_buffers.set(name, buffer);
            m_index_counts.set(name, entry.size / sizeof(uint32_t));
        }
    }

    // Load textures.
    for (const auto &entry : pack_reader.entries()) {
        switch (entry.type) {
        case vpak::EntryType::ImageData:
            auto stream = pack_reader.open(entry.name);
            m_texture_indices.set(entry.name, m_texture_images.size());
            load_image(cmd_pool, queue, *stream, staging_buffer, staging_data, memory_offset);
            break;
        }
    }
    m_context.vkUnmapMemory(staging_memory);
    m_context.vkFreeMemory(staging_memory);
    m_context.vkDestroyBuffer(staging_buffer);
}

void Scene::render(const vk::CommandBuffer &cmd_buf, vkb::PipelineLayout pipeline_layout, uint32_t cascade_index) {
    for (auto [entity, mesh, material] : m_world.view<Mesh, Material>()) {
        auto vertex_buffer = m_vertex_buffers.get(mesh.vertex_data_name());
        auto index_buffer = m_index_buffers.get(mesh.index_data_name());
        if (!vertex_buffer || !index_buffer) {
            continue;
        }

        auto albedo_index = m_texture_indices.get(material.albedo_name());
        auto normal_index = m_texture_indices.get(material.normal_name());
        if (!albedo_index || !normal_index) {
            continue;
        }

        PushConstantBlock push_constant_block{
            .transform = get_transform_matrix(entity),
            .albedo_index = *albedo_index,
            .normal_index = *normal_index,
            .cascade_index = cascade_index,
        };
        cmd_buf.bind_vertex_buffer(*vertex_buffer);
        cmd_buf.bind_index_buffer(*index_buffer, vkb::IndexType::Uint32);
        cmd_buf.push_constants(pipeline_layout, vkb::ShaderStage::All, sizeof(PushConstantBlock), &push_constant_block);
        cmd_buf.draw_indexed(*m_index_counts.get(mesh.index_data_name()), 1);
    }
}

} // namespace vull
