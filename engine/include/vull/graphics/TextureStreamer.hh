#pragma once

#include <vull/container/HashMap.hh>
#include <vull/container/Vector.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Atomic.hh>
#include <vull/support/Result.hh>
#include <vull/support/Span.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/tasklet/Mutex.hh>
#include <vull/vulkan/Buffer.hh>
#include <vull/vulkan/Image.hh>
#include <vull/vulkan/Vulkan.hh>

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
    HashMap<String, uint32_t> m_texture_indices;
    // TODO(rw-mutex)
    Mutex m_mutex;

    Vector<vk::Image> m_images;
    Mutex m_images_mutex;

    vkb::DescriptorSetLayout m_set_layout{nullptr};
    vk::Buffer m_descriptor_buffer;
    Atomic<uint32_t> m_in_progress;

    vk::Image create_default_image(Vec2u extent, vkb::Format format, Span<const uint8_t> pixel_data);
    Result<uint32_t, StreamError> load_texture(Stream &stream);
    void load_texture(String &&name);

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
