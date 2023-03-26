#pragma once

#include <vull/maths/Vec.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Tuple.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/Image.hh>

#include <stdint.h>

namespace vull::vk {

class Context;

} // namespace vull::vk

namespace vull::ui {

class Font;

struct CachedGlyph {
    Font *font;
    uint32_t index;
    Vec2u offset;
    Vec2u size;
    Vec2f bitmap_offset;
};

// Skyline Bottom-Left
class FontAtlas {
    vk::Context &m_context;
    const Vec2u m_extent;
    vk::Image m_image;

    Vector<CachedGlyph> m_cache;

    struct Node {
        Node *next{nullptr};
        Vec2u offset{};
        uint32_t width;
    };
    Node *m_skyline{nullptr};

    Optional<uint32_t> pack_rect(Node *node, Vec2u extent);
    Optional<Tuple<Node **, Vec2u>> find_rect(Vec2u extent);
    Optional<Vec2u> allocate_rect(Vec2u extent);

public:
    FontAtlas(vk::Context &context, Vec2u extent);
    FontAtlas(const FontAtlas &) = delete;
    FontAtlas(FontAtlas &&) = delete;
    ~FontAtlas();

    FontAtlas &operator=(const FontAtlas &) = delete;
    FontAtlas &operator=(FontAtlas &&) = delete;

    CachedGlyph ensure_glyph(Font &font, uint32_t glyph_index);

    Vec2u extent() const { return m_extent; }
    vk::SampledImage sampled_image() const;
};

} // namespace vull::ui
