#pragma once

#include <vull/support/Optional.hh> // IWYU pragma: keep
#include <vull/support/Span.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/tasklet/TaskletMutex.hh>

#include <stdint.h>

using FT_Face = struct FT_FaceRec_ *;
struct hb_buffer_t;
struct hb_font_t;
struct hb_glyph_info_t;
struct hb_glyph_position_t;

namespace vull::ui {

struct CachedGlyph {
    float disp_x;
    float disp_y;
};

struct ShapingPair {
    uint32_t glyph_index;
    int32_t x_advance;
    int32_t y_advance;
    int32_t x_offset;
    int32_t y_offset;
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
    hb_font_t *m_hb_font;
    mutable Vector<Optional<CachedGlyph>> m_glyph_cache;
    mutable TaskletMutex m_mutex;

public:
    explicit Font(FT_Face face);
    Font(const Font &) = delete;
    Font(Font &&other)
        : m_hb_font(vull::exchange(other.m_hb_font, nullptr)), m_glyph_cache(vull::move(other.m_glyph_cache)) {}
    ~Font();

    Font &operator=(const Font &) = delete;
    Font &operator=(Font &&) = delete;

    void rasterise(Span<float> buffer, uint32_t glyph_index) const;
    ShapingView shape(StringView text) const;
    const Optional<CachedGlyph> &cached_glyph(uint32_t glyph_index) const;
    uint32_t glyph_count() const { return m_glyph_cache.size(); }
};

} // namespace vull::ui
