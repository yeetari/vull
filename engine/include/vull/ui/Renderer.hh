#pragma once

#include <vull/maths/Vec.hh>
#include <vull/support/StringView.hh>
#include <vull/ui/GpuFont.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>
#include <sys/types.h>

using FT_Library = struct FT_LibraryRec_ *;

namespace vull::vk {

class Context;
class ImageResource;
class RenderGraph;
class Swapchain;

} // namespace vull::vk

namespace vull::ui {

enum class ObjectType {
    Rect = 0,
    TextGlyph = 1,
};

struct Object {
    Vec4f colour;
    Vec2f position;
    Vec2f scale;
    uint32_t glyph_index{0};
    ObjectType type{ObjectType::Rect};
};

class Renderer {
    const vk::Context &m_context;
    const vk::Swapchain &m_swapchain;
    FT_Library m_ft_library{nullptr};
    vkb::Sampler m_font_sampler{nullptr};
    vkb::Buffer m_ui_data_buffer{nullptr};
    vkb::DeviceMemory m_ui_data_buffer_memory{nullptr};
    vkb::DescriptorPool m_descriptor_pool{nullptr};
    vkb::DescriptorSetLayout m_descriptor_set_layout{nullptr};
    vkb::DescriptorSet m_descriptor_set{nullptr};
    vkb::PipelineLayout m_pipeline_layout{nullptr};
    vkb::Pipeline m_pipeline{nullptr};
    float m_global_scale{1.0f};

    Vec2f *m_scaling_ratio{nullptr};
    Object *m_objects{nullptr};
    uint32_t m_object_index{0};

public:
    Renderer(const vk::Context &context, vk::RenderGraph &render_graph, const vk::Swapchain &swapchain,
             vk::ImageResource &swapchain_resource, vkb::ShaderModule vertex_shader, vkb::ShaderModule fragment_shader);
    Renderer(const Renderer &) = delete;
    Renderer(Renderer &&) = delete;
    ~Renderer();

    Renderer &operator=(const Renderer &) = delete;
    Renderer &operator=(Renderer &&) = delete;

    GpuFont load_font(StringView path, ssize_t size);
    void set_global_scale(float global_scale);

    void draw_rect(const Vec4f &colour, const Vec2f &position, const Vec2f &scale);
    void draw_text(GpuFont &font, const Vec3f &colour, const Vec2f &position, StringView text);
};

} // namespace vull::ui
