#pragma once

#include <vull/container/FixedBuffer.hh>
#include <vull/container/Vector.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Optional.hh> // IWYU pragma: keep
#include <vull/support/Result.hh>
#include <vull/support/Span.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>
#include <vull/tasklet/Mutex.hh>

#include <stdint.h>

using FT_Face = struct FT_FaceRec_ *;
using FT_Library = struct FT_LibraryRec_ *;

struct hb_buffer_t;
struct hb_font_t;
struct hb_glyph_info_t;
struct hb_glyph_position_t;

namespace vull::ui {

enum class FontLoadError {
    FreetypeError,
    NotFound,
};

struct GlyphInfo {
    Vec2u bitmap_extent;
    Vec2f bitmap_offset;
};

struct ShapingPair {
    uint32_t glyph_index;
    Vec2i advance;
    Vec2i offset;
};

class ShapingIterator {
    hb_glyph_info_t *const m_glyph_infos;
    hb_glyph_position_t *const m_glyph_positions;
    unsigned m_index;

public:
    ShapingIterator(hb_glyph_info_t *glyph_infos, hb_glyph_position_t *glyph_positions, unsigned index)
        : m_glyph_infos(glyph_infos), m_glyph_positions(glyph_positions), m_index(index) {}

    ShapingIterator &operator++() {
        m_index++;
        return *this;
    }

    bool operator==(const ShapingIterator &other) const { return m_index == other.m_index; }
    ShapingPair operator*() const;
};

class ShapingView {
    hb_buffer_t *const m_buffer;
    hb_glyph_info_t *const m_glyph_infos;
    hb_glyph_position_t *const m_glyph_positions;
    const unsigned m_glyph_count;

public:
    ShapingView(hb_buffer_t *buffer, hb_glyph_info_t *glyph_infos, hb_glyph_position_t *glyph_positions,
                unsigned glyph_count)
        : m_buffer(buffer), m_glyph_infos(glyph_infos), m_glyph_positions(glyph_positions), m_glyph_count(glyph_count) {
    }
    ShapingView(const ShapingView &) = delete;
    ShapingView(ShapingView &&) = delete;
    ~ShapingView();

    ShapingView &operator=(const ShapingView &) = delete;
    ShapingView &operator=(ShapingView &&) = delete;

    ShapingIterator begin() const { return {m_glyph_infos, m_glyph_positions, 0}; }
    ShapingIterator end() const { return {m_glyph_infos, m_glyph_positions, m_glyph_count}; }
};

class Font {
    FT_Library m_library;
    ByteBuffer m_bytes;
    hb_font_t *m_hb_font;
    mutable Vector<Optional<GlyphInfo>> m_glyph_cache;
    mutable Mutex m_mutex;

public:
    static Result<Font, FontLoadError> load(StringView name, long size);

    Font(FT_Library library, ByteBuffer &&bytes, FT_Face face);
    Font(const Font &) = delete;
    Font(Font &&other)
        : m_library(vull::exchange(other.m_library, nullptr)), m_bytes(vull::move(other.m_bytes)),
          m_hb_font(vull::exchange(other.m_hb_font, nullptr)), m_glyph_cache(vull::move(other.m_glyph_cache)) {}
    ~Font();

    Font &operator=(const Font &) = delete;
    Font &operator=(Font &&) = delete;

    GlyphInfo ensure_glyph(uint32_t glyph_index) const;
    void rasterise(uint32_t glyph_index, Span<uint8_t> buffer) const;
    ShapingView shape(StringView text) const;
    Vec2u text_bounds(StringView text) const;

    uint32_t glyph_count() const { return m_glyph_cache.size(); }
};

} // namespace vull::ui
