#pragma once

#include <vull/container/Vector.hh>
#include <vull/maths/Colour.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>
#include <vull/ui/Units.hh> // IWYU pragma: keep
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
        Vec2i position;
        Vec2i size;
        Vec2f uv_a;
        Vec2f uv_c;
        Colour colour;
        uint32_t texture_index;
    };
    Vector<BoundTexture> m_bound_textures;
    Vector<Command> m_commands;
    FontAtlas *m_atlas{nullptr};

    void compile(vk::Context &context, vk::CommandBuffer &cmd_buf, Vec2u viewport_extent,
                 const vk::SampledImage &null_image);
    uint32_t get_texture_index(const vk::SampledImage &image);

public:
    Painter() { m_bound_textures.push({}); }

    void bind_atlas(FontAtlas &atlas);
    void draw_rect(LayoutPoint position, LayoutSize size, const Colour &colour);
    void draw_image(LayoutPoint position, LayoutSize size, const vk::SampledImage &image);
    void draw_text(Font &font, LayoutPoint position, const Colour &colour, StringView text);
};

} // namespace vull::ui
