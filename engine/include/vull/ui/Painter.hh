#pragma once

#include <vull/container/Vector.hh>
#include <vull/maths/Colour.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class CommandBuffer;
class Context;
class SampledImage;

} // namespace vull::vk

namespace vull::ui {

class Font;
class FontAtlas;
class Renderer;

class Painter {
    friend Renderer;

private:
    struct BoundTexture {
        vkb::ImageView view;
        vkb::Sampler sampler;
    };
    struct Command {
        Vec2f position;
        Vec2f size;
        Vec2f uv_a;
        Vec2f uv_c;
        Colour colour;
        uint32_t texture_index;
    };
    const Vec2f m_global_scale;
    Vector<BoundTexture> m_bound_textures;
    Vector<Command> m_commands;
    FontAtlas *m_atlas{nullptr};

    explicit Painter(Vec2f global_scale) : m_global_scale(global_scale) { m_bound_textures.push({}); }

    void compile(vk::Context &context, vk::CommandBuffer &cmd_buf, Vec2f viewport_extent,
                 const vk::SampledImage &null_image);
    uint32_t get_texture_index(const vk::SampledImage &image);

public:
    void bind_atlas(FontAtlas &atlas);
    void draw_rect(const Vec2f &position, const Vec2f &size, const Colour &colour);
    void draw_image(const Vec2f &position, const Vec2f &size, const vk::SampledImage &image);
    void draw_text(Font &font, Vec2f position, const Colour &colour, StringView text);

    Vec2f global_scale() const { return m_global_scale; }
};

} // namespace vull::ui