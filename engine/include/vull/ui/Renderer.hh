#pragma once

#include <vull/maths/Vec.hh>
#include <vull/ui/GpuFont.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>
#include <sys/types.h>

using FT_Library = struct FT_LibraryRec_ *;

namespace vull {

class Context;

} // namespace vull

namespace vull::ui {

struct Object {
    Vec2f position;
    uint32_t glyph_index{0};
    float padding{0.0f};
};

class Renderer {
    const Context &m_context;
    const vk::DescriptorSet m_descriptor_set;
    FT_Library m_ft_library{nullptr};
    vk::Buffer m_object_buffer{nullptr};
    vk::DeviceMemory m_object_buffer_memory{nullptr};
    vk::Sampler m_font_sampler{nullptr};

    Object *m_objects{nullptr};
    uint32_t m_object_index{0};

public:
    Renderer(const Context &context, vk::DescriptorSet descriptor_set);
    Renderer(const Renderer &) = delete;
    Renderer(Renderer &&) = delete;
    ~Renderer();

    Renderer &operator=(const Renderer &) = delete;
    Renderer &operator=(Renderer &&) = delete;

    GpuFont load_font(const char *path, ssize_t size);

    void new_frame();
    void draw_text(GpuFont &font, const Vec2u &position, const char *text);
    uint32_t object_count() const { return m_object_index; }
};

} // namespace vull::ui
