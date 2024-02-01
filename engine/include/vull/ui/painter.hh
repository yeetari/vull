#pragma once

#include <vull/container/vector.hh>
#include <vull/maths/colour.hh>
#include <vull/maths/vec.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>
#include <vull/support/variant.hh>
#include <vull/ui/units.hh> // IWYU pragma: keep
#include <vull/vulkan/vulkan.hh>

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

struct RectCommand {
    Colour colour;
};

struct ImageCommand {
    uint32_t texture_index;
};

struct TextCommand {
    Colour colour;
    Vec2f uv_a;
    Vec2f uv_c;
    uint32_t texture_index;
};

struct ScissorCommand {};

struct PaintCommand {
    Vec2i position;
    Vec2i size;
    Variant<RectCommand, ImageCommand, TextCommand, ScissorCommand> variant;
};

class Painter {
    friend Renderer;

private:
    struct BoundTexture {
        vkb::ImageView view;
        vkb::Sampler sampler;
    };
    Vector<BoundTexture> m_bound_textures;
    Vector<PaintCommand> m_commands;
    FontAtlas *m_atlas{nullptr};

    void compile(vk::Context &context, vk::CommandBuffer &cmd_buf, Vec2u viewport_extent,
                 const vk::SampledImage &null_image);
    uint32_t get_texture_index(const vk::SampledImage &image);

public:
    Painter() { m_bound_textures.push({}); }

    void bind_atlas(FontAtlas &atlas);
    void paint_rect(LayoutPoint position, LayoutSize size, const Colour &colour);
    void paint_image(LayoutPoint position, LayoutSize size, const vk::SampledImage &image);
    void paint_text(Font &font, LayoutPoint position, const Colour &colour, StringView text);
    void set_scissor(LayoutPoint position, LayoutSize size);
    void unset_scissor();
};

} // namespace vull::ui
