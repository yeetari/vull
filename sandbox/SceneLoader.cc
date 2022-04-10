#include "SceneLoader.hh"

#include <vull/core/Mesh.hh>
#include <vull/core/PackFile.hh>
#include <vull/core/PackReader.hh>
#include <vull/core/Transform.hh>
#include <vull/ecs/World.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/CommandPool.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Queue.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stddef.h>

void load_scene(vull::VkContext &context, vull::PackReader &pack_reader, vull::CommandPool &command_pool,
                vull::Queue &queue, vull::World &world, vull::Vector<vull::vk::Buffer> &vertex_buffers,
                vull::Vector<vull::vk::Buffer> &index_buffers, vull::vk::DeviceMemory mesh_memory) {
    // Read pack header and register default components. Note that the order currently matters.
    pack_reader.read_header();
    world.register_component<vull::Transform>();
    world.register_component<vull::Mesh>();

    vull::vk::BufferCreateInfo staging_buffer_ci{
        .sType = vull::vk::StructureType::BufferCreateInfo,
        .size = 1024ul * 1024ul * 4ul,
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

    size_t mesh_memory_offset = 0;
    for (auto entry = pack_reader.read_entry(); entry; entry = pack_reader.read_entry()) {
        vull::vk::BufferUsage buffer_usage;
        switch (entry->type) {
        case vull::PackEntryType::VertexData:
            buffer_usage = vull::vk::BufferUsage::VertexBuffer;
            break;
        case vull::PackEntryType::IndexData:
            buffer_usage = vull::vk::BufferUsage::IndexBuffer;
            break;
        case vull::PackEntryType::WorldData:
            world.deserialise(pack_reader);
            continue;
        default:
            continue;
        }

        VULL_ENSURE(entry->size <= staging_buffer_ci.size);
        pack_reader.read({staging_data, entry->size});

        vull::vk::BufferCreateInfo buffer_ci{
            .sType = vull::vk::StructureType::BufferCreateInfo,
            .size = entry->size,
            .usage = buffer_usage | vull::vk::BufferUsage::TransferDst,
            .sharingMode = vull::vk::SharingMode::Exclusive,
        };
        vull::vk::Buffer buffer;
        VULL_ENSURE(context.vkCreateBuffer(&buffer_ci, &buffer) == vull::vk::Result::Success);

        vull::vk::MemoryRequirements buffer_requirements{};
        context.vkGetBufferMemoryRequirements(buffer, &buffer_requirements);
        VULL_ENSURE(context.vkBindBufferMemory(buffer, mesh_memory, mesh_memory_offset) == vull::vk::Result::Success);

        command_pool.begin(vull::vk::CommandPoolResetFlags::None);
        auto cmd_buf = command_pool.request_cmd_buf();
        vull::vk::BufferCopy copy{
            .size = entry->size,
        };
        cmd_buf.copy_buffer(staging_buffer, buffer, {&copy, 1});
        queue.submit(cmd_buf, nullptr, {}, {});
        queue.wait_idle();

        switch (entry->type) {
        case vull::PackEntryType::VertexData:
            vertex_buffers.push(buffer);
            break;
        case vull::PackEntryType::IndexData:
            index_buffers.push(buffer);
            break;
        }
        mesh_memory_offset += entry->size;
        mesh_memory_offset =
            (mesh_memory_offset + buffer_requirements.alignment - 1) & ~(buffer_requirements.alignment - 1);
    }

    context.vkUnmapMemory(staging_memory);
    context.vkFreeMemory(staging_memory);
    context.vkDestroyBuffer(staging_buffer);
}
