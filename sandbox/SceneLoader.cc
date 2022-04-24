#include "SceneLoader.hh"

#include <vull/core/Material.hh>
#include <vull/core/Mesh.hh>
#include <vull/core/Transform.hh>
#include <vull/ecs/World.hh>
#include <vull/maths/Common.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Vector.hh>
#include <vull/vpak/PackFile.hh>
#include <vull/vpak/PackReader.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Queue.hh>
#include <vull/vulkan/Vulkan.hh>
// IWYU pragma: no_forward_declare vull::vk::Image_T

#include <stdint.h>
#include <stdio.h>

namespace {

constexpr auto k_staging_buffer_size = 1024ul * 1024ul * 4ul;

struct FormatPair {
    vull::vk::Format format;
    uint32_t unit_size;
    bool block_compressed;
};

FormatPair vk_format(vull::PackImageFormat pack_format) {
    switch (pack_format) {
    case vull::PackImageFormat::Bc1Srgb:
        return {vull::vk::Format::Bc1RgbaSrgbBlock, uint32_t(8), true};
    case vull::PackImageFormat::Bc3Srgb:
        return {vull::vk::Format::Bc3SrgbBlock, uint32_t(16), true};
    case vull::PackImageFormat::Bc5Unorm:
        return {vull::vk::Format::Bc5UnormBlock, uint32_t(16), true};
    case vull::PackImageFormat::RgUnorm:
        return {vull::vk::Format::R8G8Unorm, uint32_t(2), false};
    case vull::PackImageFormat::RgbaUnorm:
        return {vull::vk::Format::R8G8B8A8Unorm, uint32_t(4), false};
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

vull::vk::Buffer load_buffer(vull::VkContext &context, vull::PackReader &pack_reader, vull::CommandPool &command_pool,
                             vull::Queue &queue, vull::vk::Buffer staging_buffer, void *staging_data,
                             vull::vk::DeviceMemory memory, vull::vk::DeviceSize &memory_offset, uint32_t size,
                             vull::vk::BufferUsage usage) {
    vull::vk::BufferCreateInfo buffer_ci{
        .sType = vull::vk::StructureType::BufferCreateInfo,
        .size = size,
        .usage = usage | vull::vk::BufferUsage::TransferDst,
        .sharingMode = vull::vk::SharingMode::Exclusive,
    };
    vull::vk::Buffer buffer;
    VULL_ENSURE(context.vkCreateBuffer(&buffer_ci, &buffer) == vull::vk::Result::Success);

    vull::vk::MemoryRequirements memory_requirements{};
    context.vkGetBufferMemoryRequirements(buffer, &memory_requirements);
    memory_offset = (memory_offset + memory_requirements.alignment - 1) & ~(memory_requirements.alignment - 1);
    VULL_ENSURE(context.vkBindBufferMemory(buffer, memory, memory_offset) == vull::vk::Result::Success);

    VULL_ENSURE(size <= k_staging_buffer_size);
    pack_reader.read({staging_data, size});

    queue.immediate_submit(command_pool, [=](const vull::CommandBuffer &cmd_buf) {
        vull::vk::BufferCopy copy{
            .size = size,
        };
        cmd_buf.copy_buffer(staging_buffer, buffer, {&copy, 1});
    });
    memory_offset += size;
    return buffer;
}

void load_image(vull::VkContext &context, vull::PackReader &pack_reader, vull::CommandPool &command_pool,
                vull::Queue &queue, vull::Vector<vull::vk::Image> &images,
                vull::Vector<vull::vk::ImageView> &image_views, vull::vk::Buffer staging_buffer, void *staging_data,
                vull::vk::DeviceMemory memory, vull::vk::DeviceSize &memory_offset) {
    const auto [format, unit_size, block_compressed] = vk_format(vull::PackImageFormat(pack_reader.read_byte()));
    const auto width = pack_reader.read_varint();
    const auto height = pack_reader.read_varint();
    const auto mip_count = pack_reader.read_varint();

    // TODO: What's the best thing to do if this happens?
    uint32_t expected_mip_count = 32u - vull::clz(vull::max(width, height));
    if (mip_count != expected_mip_count) {
        fprintf(stderr, "warning: expected %u mips, but got %u\n", expected_mip_count, mip_count);
    }

    vull::vk::ImageCreateInfo image_ci{
        .sType = vull::vk::StructureType::ImageCreateInfo,
        .imageType = vull::vk::ImageType::_2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = mip_count,
        .arrayLayers = 1,
        .samples = vull::vk::SampleCount::_1,
        .tiling = vull::vk::ImageTiling::Optimal,
        .usage = vull::vk::ImageUsage::TransferDst | vull::vk::ImageUsage::Sampled,
        .sharingMode = vull::vk::SharingMode::Exclusive,
        .initialLayout = vull::vk::ImageLayout::Undefined,
    };
    auto &image = images.emplace();
    VULL_ENSURE(context.vkCreateImage(&image_ci, &image) == vull::vk::Result::Success);

    vull::vk::MemoryRequirements memory_requirements{};
    context.vkGetImageMemoryRequirements(image, &memory_requirements);
    memory_offset = (memory_offset + memory_requirements.alignment - 1) & ~(memory_requirements.alignment - 1);
    VULL_ENSURE(context.vkBindImageMemory(image, memory, memory_offset) == vull::vk::Result::Success);

    vull::vk::ImageViewCreateInfo image_view_ci{
        .sType = vull::vk::StructureType::ImageViewCreateInfo,
        .image = image,
        .viewType = vull::vk::ImageViewType::_2D,
        .format = format,
        .subresourceRange{
            .aspectMask = vull::vk::ImageAspect::Color,
            .levelCount = mip_count,
            .layerCount = 1,
        },
    };
    VULL_ENSURE(context.vkCreateImageView(&image_view_ci, &image_views.emplace()) == vull::vk::Result::Success);

    // Transition whole image to TransferDstOptimal.
    queue.immediate_submit(command_pool, [image, mip_count](const vull::CommandBuffer &cmd_buf) {
        vull::vk::ImageMemoryBarrier transfer_write_barrier{
            .sType = vull::vk::StructureType::ImageMemoryBarrier,
            .dstAccessMask = vull::vk::Access::TransferWrite,
            .oldLayout = vull::vk::ImageLayout::Undefined,
            .newLayout = vull::vk::ImageLayout::TransferDstOptimal,
            .image = image,
            .subresourceRange{
                .aspectMask = vull::vk::ImageAspect::Color,
                .levelCount = mip_count,
                .layerCount = 1,
            },
        };
        cmd_buf.pipeline_barrier(vull::vk::PipelineStage::None, vull::vk::PipelineStage::Transfer, {},
                                 {&transfer_write_barrier, 1});
    });

    uint32_t mip_width = width;
    uint32_t mip_height = height;
    for (uint32_t i = 0; i < mip_count; i++) {
        const uint32_t mip_size = block_compressed ? ((mip_width + 3) / 4) * ((mip_height + 3) / 4) * unit_size
                                                   : mip_width * mip_height * unit_size;
        VULL_ENSURE(mip_size <= k_staging_buffer_size);
        pack_reader.read({staging_data, mip_size});

        // Perform CPU -> GPU copy.
        queue.immediate_submit(command_pool, [=](const vull::CommandBuffer &cmd_buf) {
            vull::vk::BufferImageCopy copy{
                .imageSubresource{
                    .aspectMask = vull::vk::ImageAspect::Color,
                    .mipLevel = i,
                    .layerCount = 1,
                },
                .imageExtent = {mip_width, mip_height, 1},
            };
            cmd_buf.copy_buffer_to_image(staging_buffer, image, vull::vk::ImageLayout::TransferDstOptimal, {&copy, 1});
        });
        mip_width /= 2;
        mip_height /= 2;
        memory_offset += mip_size;
    }

    // Transition whole image to ShaderReadOnlyOptimal.
    queue.immediate_submit(command_pool, [image, mip_count](const vull::CommandBuffer &cmd_buf) {
        vull::vk::ImageMemoryBarrier image_read_barrier{
            .sType = vull::vk::StructureType::ImageMemoryBarrier,
            .srcAccessMask = vull::vk::Access::TransferWrite,
            .dstAccessMask = vull::vk::Access::ShaderRead,
            .oldLayout = vull::vk::ImageLayout::TransferDstOptimal,
            .newLayout = vull::vk::ImageLayout::ShaderReadOnlyOptimal,
            .image = image,
            .subresourceRange{
                .aspectMask = vull::vk::ImageAspect::Color,
                .levelCount = mip_count,
                .layerCount = 1,
            },
        };
        cmd_buf.pipeline_barrier(vull::vk::PipelineStage::Transfer, vull::vk::PipelineStage::AllCommands, {},
                                 {&image_read_barrier, 1});
    });
}

} // namespace

void load_scene(vull::VkContext &context, vull::PackReader &pack_reader, vull::CommandPool &command_pool,
                vull::Queue &queue, vull::World &world, vull::Vector<vull::vk::Buffer> &vertex_buffers,
                vull::Vector<vull::vk::Buffer> &index_buffers, vull::Vector<vull::vk::Image> &images,
                vull::Vector<vull::vk::ImageView> &image_views, vull::vk::DeviceMemory memory) {
    // Read pack header and register default components. Note that the order currently matters.
    pack_reader.read_header();
    world.register_component<vull::Transform>();
    world.register_component<vull::Mesh>();
    world.register_component<vull::Material>();

    vull::vk::BufferCreateInfo staging_buffer_ci{
        .sType = vull::vk::StructureType::BufferCreateInfo,
        .size = k_staging_buffer_size,
        .usage = vull::vk::BufferUsage::TransferSrc,
        .sharingMode = vull::vk::SharingMode::Exclusive,
    };
    vull::vk::Buffer staging_buffer;
    VULL_ENSURE(context.vkCreateBuffer(&staging_buffer_ci, &staging_buffer) == vull::vk::Result::Success);

    vull::vk::MemoryRequirements staging_memory_requirements{};
    context.vkGetBufferMemoryRequirements(staging_buffer, &staging_memory_requirements);
    auto *staging_memory = context.allocate_memory(staging_memory_requirements, vull::MemoryType::Staging);
    VULL_ENSURE(context.vkBindBufferMemory(staging_buffer, staging_memory, 0) == vull::vk::Result::Success);

    void *staging_data;
    VULL_ENSURE(context.vkMapMemory(staging_memory, 0, vull::vk::k_whole_size, 0, &staging_data) ==
                vull::vk::Result::Success);

    vull::vk::DeviceSize memory_offset = 0;
    for (auto entry = pack_reader.read_entry(); entry; entry = pack_reader.read_entry()) {
        switch (entry->type) {
        case vull::PackEntryType::VertexData:
            vertex_buffers.push(load_buffer(context, pack_reader, command_pool, queue, staging_buffer, staging_data,
                                            memory, memory_offset, entry->size, vull::vk::BufferUsage::VertexBuffer));
            break;
        case vull::PackEntryType::IndexData:
            index_buffers.push(load_buffer(context, pack_reader, command_pool, queue, staging_buffer, staging_data,
                                           memory, memory_offset, entry->size, vull::vk::BufferUsage::IndexBuffer));
            break;
        case vull::PackEntryType::ImageData:
            load_image(context, pack_reader, command_pool, queue, images, image_views, staging_buffer, staging_data,
                       memory, memory_offset);
            break;
        case vull::PackEntryType::WorldData:
            world.deserialise(pack_reader);
            break;
        }
    }
    context.vkUnmapMemory(staging_memory);
    context.vkFreeMemory(staging_memory);
    context.vkDestroyBuffer(staging_buffer);
}
