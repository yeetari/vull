#pragma once

#include <vull/maths/Vec.hh>
#include <vull/ui/GpuFont.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>
#include <sys/types.h>

using FT_Library = struct FT_LibraryRec_ *;

namespace vull {

class Context;
class Swapchain;

} // namespace vull

namespace vull::ui {

enum class ObjectType {
    Rect = 0,
    RectOutline = 1,
    TextGlyph = 2,
};

struct Object {
    Vec4f colour;
    Vec2f position;
    Vec2f scale;
    uint32_t glyph_index{0};
    ObjectType type{ObjectType::Rect};
};

class Renderer {
    const Context &m_context;
    const Swapchain &m_swapchain;
    FT_Library m_ft_library{nullptr};
    vk::Sampler m_font_sampler{nullptr};
    vk::Buffer m_object_buffer{nullptr};
    vk::DeviceMemory m_object_buffer_memory{nullptr};
    vk::DescriptorPool m_descriptor_pool{nullptr};
    vk::DescriptorSetLayout m_descriptor_set_layout{nullptr};
    vk::DescriptorSet m_descriptor_set{nullptr};
    vk::PipelineLayout m_pipeline_layout{nullptr};
    vk::Pipeline m_pipeline{nullptr};

    Object *m_objects{nullptr};
    uint32_t m_object_index{0};

public:
    Renderer(const Context &context, const Swapchain &swapchain, vk::ShaderModule vertex_shader,
             vk::ShaderModule fragment_shader);
    Renderer(const Renderer &) = delete;
    Renderer(Renderer &&) = delete;
    ~Renderer();

    Renderer &operator=(const Renderer &) = delete;
    Renderer &operator=(Renderer &&) = delete;

    GpuFont load_font(const char *path, ssize_t size);

    void draw_rect(const Vec4f &colour, const Vec2f &position, const Vec2f &scale, bool fill = true);
    void draw_text(GpuFont &font, const Vec3f &colour, const Vec2u &position, const char *text);
    void render(vk::CommandBuffer command_buffer, uint32_t image_index);
};

} // namespace vull::ui
