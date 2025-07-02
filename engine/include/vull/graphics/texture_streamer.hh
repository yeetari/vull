#pragma once

#include <vull/container/hash_map.hh>
#include <vull/container/vector.hh>
#include <vull/maths/vec.hh>
#include <vull/support/result.hh>
#include <vull/support/span.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/tasklet/future.hh>
#include <vull/tasklet/mutex.hh>
#include <vull/vulkan/buffer.hh>
#include <vull/vulkan/image.hh>
#include <vull/vulkan/vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Context;

} // namespace vull::vk

namespace vull {

enum class StreamError;
struct Stream;

enum class TextureKind {
    Albedo,
    Normal,
};

class TextureStreamer {
    vk::Context &m_context;
    HashMap<String, uint32_t> m_loaded_indices;
    HashMap<String, tasklet::Future<uint32_t>> m_futures;

    Vector<vk::Image> m_images;
    tasklet::Mutex m_images_mutex;

    vkb::DescriptorSetLayout m_set_layout{nullptr};
    vk::Buffer m_descriptor_buffer;

    vk::Image create_default_image(Vec2u extent, vkb::Format format, Span<const uint8_t> pixel_data);
    Result<uint32_t, StreamError> load_texture(Stream &stream);
    uint32_t load_texture(String &&name, uint32_t fallback_index);

public:
    explicit TextureStreamer(vk::Context &context);
    TextureStreamer(const TextureStreamer &) = delete;
    TextureStreamer(TextureStreamer &&) = delete;
    ~TextureStreamer();

    TextureStreamer &operator=(const TextureStreamer &) = delete;
    TextureStreamer &operator=(TextureStreamer &&) = delete;

    uint32_t ensure_texture(StringView name, TextureKind kind);

    vkb::DescriptorSetLayout set_layout() const { return m_set_layout; }
    const vk::Buffer &descriptor_buffer() const { return m_descriptor_buffer; }
};

} // namespace vull
