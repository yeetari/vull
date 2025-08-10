#pragma once

#include <vull/container/hash_map.hh>
#include <vull/support/atomic.hh>
#include <vull/support/optional.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/tasklet/future.hh>
#include <vull/vulkan/buffer.hh>
#include <vull/vulkan/vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Context;

} // namespace vull::vk

namespace vull {

struct MeshInfo {
    uint32_t index_count;
    uint32_t index_offset;
    int32_t vertex_offset;
};

class MeshStreamer {
    vk::Context &m_context;
    vkb::DeviceSize m_vertex_size;

    vk::Buffer m_vertex_buffer;
    vk::Buffer m_index_buffer;

    HashMap<String, MeshInfo> m_loaded_meshes;
    HashMap<String, tasklet::Future<MeshInfo>> m_futures;
    Atomic<vkb::DeviceSize> m_vertex_buffer_head;
    Atomic<vkb::DeviceSize> m_index_buffer_head;

    MeshInfo load_mesh(const String &name);

public:
    MeshStreamer(vk::Context &context, vkb::DeviceSize vertex_size);
    MeshStreamer(const MeshStreamer &) = delete;
    MeshStreamer(MeshStreamer &&) = delete;
    ~MeshStreamer();

    MeshStreamer &operator=(const MeshStreamer &) = delete;
    MeshStreamer &operator=(MeshStreamer &&) = delete;

    Optional<MeshInfo> ensure_mesh(const String &name);

    vk::Buffer &vertex_buffer() { return m_vertex_buffer; }
    vk::Buffer &index_buffer() { return m_index_buffer; }
};

} // namespace vull
