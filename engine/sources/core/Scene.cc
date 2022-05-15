#include <vull/core/Scene.hh>

#include <vull/core/Material.hh>
#include <vull/core/Mesh.hh>
#include <vull/core/Transform.hh>
#include <vull/ecs/Entity.hh>
#include <vull/ecs/EntityId.hh>
#include <vull/ecs/World.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Mat.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Tuple.hh>
#include <vull/support/Vector.hh>
#include <vull/vpak/PackFile.hh>
#include <vull/vpak/PackReader.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Queue.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdio.h>

// IWYU pragma: no_forward_declare vull::vk::Image_T

namespace vull {
namespace {

constexpr auto k_staging_buffer_size = 1024ul * 1024ul * 4ul;

struct FormatInfo {
    vk::Format format;
    uint32_t unit_size;
    bool block_compressed;
};

FormatInfo parse_format(uint8_t pack_format) {
    switch (PackImageFormat(pack_format)) {
    case PackImageFormat::Bc1Srgb:
        return {vk::Format::Bc1RgbaSrgbBlock, 8u, true};
    case PackImageFormat::Bc3Srgb:
        return {vk::Format::Bc3SrgbBlock, 16u, true};
    case PackImageFormat::Bc5Unorm:
        return {vk::Format::Bc5UnormBlock, 16u, true};
    case PackImageFormat::RgUnorm:
        return {vk::Format::R8G8Unorm, 2u, false};
    case PackImageFormat::RgbaUnorm:
        return {vk::Format::R8G8B8A8Unorm, 4u, false};
    default:
        return {vk::Format::Undefined, 0u, false};
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
    for (auto *index_buffer : m_index_buffers) {
        m_context.vkDestroyBuffer(index_buffer);
    }
    for (auto *vertex_buffer : m_vertex_buffers) {
        m_context.vkDestroyBuffer(vertex_buffer);
    }
    m_context.vkFreeMemory(m_memory);
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

vk::Buffer Scene::load_buffer(CommandPool &cmd_pool, Queue &queue, PackReader &pack_reader, vk::Buffer staging_buffer,
                              void *staging_data, vk::DeviceSize &memory_offset, uint32_t size, vk::BufferUsage usage) {
    vk::BufferCreateInfo buffer_ci{
        .sType = vk::StructureType::BufferCreateInfo,
        .size = size,
        .usage = usage | vk::BufferUsage::TransferDst,
        .sharingMode = vk::SharingMode::Exclusive,
    };
    vk::Buffer buffer;
    VULL_ENSURE(m_context.vkCreateBuffer(&buffer_ci, &buffer) == vk::Result::Success);

    vk::MemoryRequirements memory_requirements{};
    m_context.vkGetBufferMemoryRequirements(buffer, &memory_requirements);
    memory_offset = (memory_offset + memory_requirements.alignment - 1) & ~(memory_requirements.alignment - 1);
    VULL_ENSURE(m_context.vkBindBufferMemory(buffer, m_memory, memory_offset) == vk::Result::Success);

    VULL_ENSURE(size <= k_staging_buffer_size);
    pack_reader.read({staging_data, size});

    queue.immediate_submit(cmd_pool, [=](const CommandBuffer &cmd_buf) {
        vk::BufferCopy copy{
            .size = size,
        };
        cmd_buf.copy_buffer(staging_buffer, buffer, copy);
    });
    memory_offset += size;
    return buffer;
}

void Scene::load_image(CommandPool &cmd_pool, Queue &queue, PackReader &pack_reader, vk::Buffer staging_buffer,
                       void *staging_data, vk::DeviceSize &memory_offset) {
    const auto [format, unit_size, block_compressed] = parse_format(pack_reader.read_byte());
    const auto width = pack_reader.read_varint();
    const auto height = pack_reader.read_varint();
    const auto mip_count = pack_reader.read_varint();

    // TODO: What's the best thing to do if this happens?
    uint32_t expected_mip_count = 32u - vull::clz(vull::max(width, height));
    if (mip_count != expected_mip_count) {
        fprintf(stderr, "warning: expected %u mips, but got %u\n", expected_mip_count, mip_count);
    }

    vk::ImageCreateInfo image_ci{
        .sType = vk::StructureType::ImageCreateInfo,
        .imageType = vk::ImageType::_2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = mip_count,
        .arrayLayers = 1,
        .samples = vk::SampleCount::_1,
        .tiling = vk::ImageTiling::Optimal,
        .usage = vk::ImageUsage::TransferDst | vk::ImageUsage::Sampled,
        .sharingMode = vk::SharingMode::Exclusive,
        .initialLayout = vk::ImageLayout::Undefined,
    };
    auto &image = m_texture_images.emplace();
    VULL_ENSURE(m_context.vkCreateImage(&image_ci, &image) == vk::Result::Success);

    vk::MemoryRequirements memory_requirements{};
    m_context.vkGetImageMemoryRequirements(image, &memory_requirements);
    memory_offset = (memory_offset + memory_requirements.alignment - 1) & ~(memory_requirements.alignment - 1);
    VULL_ENSURE(m_context.vkBindImageMemory(image, m_memory, memory_offset) == vk::Result::Success);

    vk::ImageViewCreateInfo image_view_ci{
        .sType = vk::StructureType::ImageViewCreateInfo,
        .image = image,
        .viewType = vk::ImageViewType::_2D,
        .format = format,
        .subresourceRange{
            .aspectMask = vk::ImageAspect::Color,
            .levelCount = mip_count,
            .layerCount = 1,
        },
    };
    VULL_ENSURE(m_context.vkCreateImageView(&image_view_ci, &m_texture_views.emplace()) == vk::Result::Success);

    // Transition the whole image (all mip levels) to TransferDstOptimal.
    queue.immediate_submit(cmd_pool, [image, mip_count](const CommandBuffer &cmd_buf) {
        vk::ImageMemoryBarrier transfer_write_barrier{
            .sType = vk::StructureType::ImageMemoryBarrier,
            .dstAccessMask = vk::Access::TransferWrite,
            .oldLayout = vk::ImageLayout::Undefined,
            .newLayout = vk::ImageLayout::TransferDstOptimal,
            .image = image,
            .subresourceRange{
                .aspectMask = vk::ImageAspect::Color,
                .levelCount = mip_count,
                .layerCount = 1,
            },
        };
        cmd_buf.pipeline_barrier(vk::PipelineStage::None, vk::PipelineStage::Transfer, {}, transfer_write_barrier);
    });

    uint32_t mip_width = width;
    uint32_t mip_height = height;
    for (uint32_t i = 0; i < mip_count; i++) {
        const uint32_t mip_size = block_compressed ? ((mip_width + 3) / 4) * ((mip_height + 3) / 4) * unit_size
                                                   : mip_width * mip_height * unit_size;
        VULL_ENSURE(mip_size <= k_staging_buffer_size);
        pack_reader.read({staging_data, mip_size});

        // Perform CPU -> GPU copy.
        queue.immediate_submit(cmd_pool, [=](const CommandBuffer &cmd_buf) {
            vk::BufferImageCopy copy{
                .imageSubresource{
                    .aspectMask = vk::ImageAspect::Color,
                    .mipLevel = i,
                    .layerCount = 1,
                },
                .imageExtent = {mip_width, mip_height, 1},
            };
            cmd_buf.copy_buffer_to_image(staging_buffer, image, vk::ImageLayout::TransferDstOptimal, copy);
        });
        mip_width /= 2;
        mip_height /= 2;
        memory_offset += mip_size;
    }

    // Transition the whole image to ShaderReadOnlyOptimal.
    queue.immediate_submit(cmd_pool, [image, mip_count](const CommandBuffer &cmd_buf) {
        vk::ImageMemoryBarrier image_read_barrier{
            .sType = vk::StructureType::ImageMemoryBarrier,
            .srcAccessMask = vk::Access::TransferWrite,
            .dstAccessMask = vk::Access::ShaderRead,
            .oldLayout = vk::ImageLayout::TransferDstOptimal,
            .newLayout = vk::ImageLayout::ShaderReadOnlyOptimal,
            .image = image,
            .subresourceRange{
                .aspectMask = vk::ImageAspect::Color,
                .levelCount = mip_count,
                .layerCount = 1,
            },
        };
        cmd_buf.pipeline_barrier(vk::PipelineStage::Transfer, vk::PipelineStage::AllCommands, {}, image_read_barrier);
    });
}

void Scene::load(CommandPool &cmd_pool, Queue &queue, FILE *pack_file) {
    // For now, allocate a fixed amount of VRAM to store all the scene's resources in.
    vk::MemoryRequirements memory_requirements{
        .size = 1024ul * 1024ul * 2048ul,
        .memoryTypeBits = 0xffffffffu,
    };
    m_memory = m_context.allocate_memory(memory_requirements, MemoryType::DeviceLocal);

    // Read pack header and register default components. Note that the order currently matters.
    PackReader pack_reader(pack_file);
    pack_reader.read_header();
    m_world.register_component<Transform>();
    m_world.register_component<Mesh>();
    m_world.register_component<Material>();

    // Allocate a temporary staging buffer used to upload data to VRAM.
    vk::BufferCreateInfo staging_buffer_ci{
        .sType = vk::StructureType::BufferCreateInfo,
        .size = k_staging_buffer_size,
        .usage = vk::BufferUsage::TransferSrc,
        .sharingMode = vk::SharingMode::Exclusive,
    };
    vk::Buffer staging_buffer;
    VULL_ENSURE(m_context.vkCreateBuffer(&staging_buffer_ci, &staging_buffer) == vk::Result::Success);
    vk::MemoryRequirements staging_memory_requirements{};
    m_context.vkGetBufferMemoryRequirements(staging_buffer, &staging_memory_requirements);
    auto *staging_memory = m_context.allocate_memory(staging_memory_requirements, MemoryType::Staging);
    VULL_ENSURE(m_context.vkBindBufferMemory(staging_buffer, staging_memory, 0) == vk::Result::Success);
    void *staging_data;
    VULL_ENSURE(m_context.vkMapMemory(staging_memory, 0, vk::k_whole_size, 0, &staging_data) == vk::Result::Success);

    // Read pack entries sequentially.
    vk::DeviceSize memory_offset = 0;
    for (auto entry = pack_reader.read_entry(); entry; entry = pack_reader.read_entry()) {
        switch (entry->type) {
        case PackEntryType::VertexData:
            m_vertex_buffers.push(load_buffer(cmd_pool, queue, pack_reader, staging_buffer, staging_data, memory_offset,
                                              entry->size, vk::BufferUsage::VertexBuffer));
            break;
        case PackEntryType::IndexData:
            m_index_buffers.push(load_buffer(cmd_pool, queue, pack_reader, staging_buffer, staging_data, memory_offset,
                                             entry->size, vk::BufferUsage::IndexBuffer));
            break;
        case PackEntryType::ImageData:
            load_image(cmd_pool, queue, pack_reader, staging_buffer, staging_data, memory_offset);
            break;
        case PackEntryType::WorldData:
            m_world.deserialise(pack_reader);
            break;
        }
    }
    m_context.vkUnmapMemory(staging_memory);
    m_context.vkFreeMemory(staging_memory);
    m_context.vkDestroyBuffer(staging_buffer);
}

void Scene::render(const CommandBuffer &cmd_buf, vk::PipelineLayout pipeline_layout, uint32_t cascade_index) {
    for (auto [entity, mesh, material] : m_world.view<Mesh, Material>()) {
        PushConstantBlock push_constant_block{
            .transform = get_transform_matrix(entity),
            .albedo_index = material.albedo_index(),
            .normal_index = material.normal_index(),
            .cascade_index = cascade_index,
        };
        cmd_buf.bind_vertex_buffer(m_vertex_buffers[mesh.index()]);
        cmd_buf.bind_index_buffer(m_index_buffers[mesh.index()], vk::IndexType::Uint32);
        cmd_buf.push_constants(pipeline_layout, vk::ShaderStage::All, sizeof(PushConstantBlock), &push_constant_block);
        cmd_buf.draw_indexed(mesh.index_count(), 1);
    }
}

} // namespace vull
