#include <vull/graphics/mesh_streamer.hh>

#include <vull/container/hash_map.hh>
#include <vull/core/log.hh>
#include <vull/core/tracing.hh>
#include <vull/support/assert.hh>
#include <vull/support/atomic.hh>
#include <vull/support/optional.hh>
#include <vull/support/result.hh>
#include <vull/support/span.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/functions.hh>
#include <vull/tasklet/future.hh>
#include <vull/vpak/file_system.hh>
#include <vull/vpak/stream.hh>
#include <vull/vulkan/buffer.hh>
#include <vull/vulkan/command_buffer.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/memory_usage.hh>
#include <vull/vulkan/queue.hh>
#include <vull/vulkan/vulkan.hh>

#include <stdint.h>

namespace vull {
namespace {

constexpr uint32_t k_in_flight_limit = 32;

} // namespace

MeshStreamer::MeshStreamer(vk::Context &context, vkb::DeviceSize vertex_size)
    : m_context(context), m_vertex_size(vertex_size) {
    m_vertex_buffer =
        m_context.create_buffer(1024uz * 1024 * 64, vkb::BufferUsage::TransferDst | vkb::BufferUsage::StorageBuffer,
                                vk::MemoryUsage::DeviceOnly);
    m_index_buffer = m_context.create_buffer(
        1024uz * 1024 * 64, vkb::BufferUsage::TransferDst | vkb::BufferUsage::IndexBuffer, vk::MemoryUsage::DeviceOnly);
}

MeshStreamer::~MeshStreamer() {
    // Wait for any in progress uploads to complete.
    for (auto &[_, future] : m_futures) {
        future.await();
    }
}

MeshInfo MeshStreamer::load_mesh(const String &name) {
    auto data_stream = vpak::open(name);
    if (!data_stream) {
        vull::error("[graphics] Failed to find mesh '{}'", name);
        return {};
    }

    const auto vertices_size = VULL_EXPECT(data_stream->read_varint<uint64_t>());
    const auto indices_size = VULL_EXPECT(data_stream->read_varint<uint64_t>());
    auto staging_buffer =
        m_context.create_buffer(vertices_size + indices_size, vkb::BufferUsage::TransferSrc, vk::MemoryUsage::HostOnly);
    VULL_EXPECT(data_stream->read({staging_buffer.mapped_raw(), staging_buffer.size()}));

    const auto vertex_buffer_offset = m_vertex_buffer_head.fetch_add(vertices_size, vull::memory_order_relaxed);
    const auto index_buffer_offset = m_index_buffer_head.fetch_add(indices_size, vull::memory_order_relaxed);
    VULL_ENSURE(vertex_buffer_offset + vertices_size < m_vertex_buffer.size());
    VULL_ENSURE(index_buffer_offset + indices_size < m_index_buffer.size());

    auto &queue = m_context.get_queue(vk::QueueKind::Transfer);
    auto cmd_buf = queue.request_cmd_buf();

    vkb::BufferCopy vertex_copy{
        .dstOffset = vertex_buffer_offset,
        .size = vertices_size,
    };
    cmd_buf->copy_buffer(staging_buffer, m_vertex_buffer, vertex_copy);

    vkb::BufferCopy index_copy{
        .srcOffset = vertices_size,
        .dstOffset = index_buffer_offset,
        .size = indices_size,
    };
    cmd_buf->copy_buffer(staging_buffer, m_index_buffer, index_copy);

    cmd_buf->bind_associated_buffer(vull::move(staging_buffer));
    queue.submit(vull::move(cmd_buf), {}, {}).await();

    return MeshInfo{
        .index_count = static_cast<uint32_t>(indices_size / sizeof(uint32_t)),
        .index_offset = static_cast<uint32_t>(index_buffer_offset / sizeof(uint32_t)),
        .vertex_offset = static_cast<int32_t>(vertex_buffer_offset / m_vertex_size),
    };
}

Optional<MeshInfo> MeshStreamer::ensure_mesh(const String &name) {
    // First check if the mesh is already loaded.
    if (auto index = m_loaded_meshes.get(name)) {
        return *index;
    }

    // Check if there is a pending future.
    if (auto future = m_futures.get(name)) {
        if (!future->is_complete()) {
            // The mesh is still being loaded.
            return vull::nullopt;
        }
        const auto mesh_info = future->await();
        m_loaded_meshes.set(name, mesh_info);
        m_futures.remove(name);
        return mesh_info;
    }

    // Don't schedule the stream just yet if there's already a lot in flight.
    if (m_futures.size() >= k_in_flight_limit) {
        return vull::nullopt;
    }

    // There is no pending future so we need to schedule the stream.
    auto future = tasklet::schedule([this, name] {
        tracing::ScopedTrace trace("Stream Mesh");
        trace.add_text(name);
        return load_mesh(name);
    });
    m_futures.set(name, vull::move(future));
    return vull::nullopt;
}

} // namespace vull
