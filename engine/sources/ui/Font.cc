#include <vull/ui/Font.hh>

#include <vull/maths/Common.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Span.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Vector.hh>
#include <vull/tasklet/Tasklet.hh>

#include <ft2build.h> // IWYU pragma: keep
// IWYU pragma: no_include "freetype/config/ftheader.h"

#include <freetype/freetype.h>
#include <freetype/fterrors.h>
#include <freetype/ftimage.h>
#include <hb-ft.h>
#include <hb.h>

namespace vull::ui {

ShapingPair ShapingIterator::operator*() const {
    return ShapingPair{
        .glyph_index = m_glyph_infos[m_index].codepoint,
        .x_advance = m_glyph_positions[m_index].x_advance,
        .y_advance = m_glyph_positions[m_index].y_advance,
        .x_offset = m_glyph_positions[m_index].x_offset,
        .y_offset = m_glyph_positions[m_index].y_offset,
    };
}

ShapingView::~ShapingView() {
    hb_buffer_destroy(m_buffer);
}

Font::Font(FT_Face face) : m_hb_font(hb_ft_font_create_referenced(face)) {
    hb_ft_font_set_funcs(m_hb_font);
    m_glyph_cache.ensure_size(static_cast<uint32_t>(face->num_glyphs));
}

Font::~Font() {
    hb_font_destroy(m_hb_font);
}

void Font::rasterise(Span<float> buffer, uint32_t glyph_index) const {
    if (m_glyph_cache[glyph_index]) {
        return;
    }
    m_glyph_cache[glyph_index].emplace(CachedGlyph{
        .disp_x = 0.0f,
        .disp_y = 0.0f,
    });
    schedule(
        [this, buffer, glyph_index] {
            auto *face = hb_ft_font_get_face(m_hb_font);
            if (FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT) != FT_Err_Ok) {
                return;
            }
            m_glyph_cache[glyph_index]->disp_x = static_cast<float>(face->glyph->bitmap_left);
            m_glyph_cache[glyph_index]->disp_y = -static_cast<float>(face->glyph->bitmap_top);
            if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_SDF) != FT_Err_Ok) {
                return;
            }
            const auto width = static_cast<uint32_t>(vull::sqrt(static_cast<float>(buffer.size())));
            const auto &bitmap = face->glyph->bitmap;
            for (unsigned y = 0; y < bitmap.rows; y++) {
                for (unsigned x = 0; x < bitmap.width; x++) {
                    const auto pixel = bitmap.buffer[y * static_cast<unsigned>(bitmap.pitch) + x];
                    buffer.begin()[y * width + x] = static_cast<float>(pixel) / 256.0f;
                }
            }
        },
        m_semaphore);
}

ShapingView Font::shape(StringView text) const {
    hb_buffer_t *buffer = hb_buffer_create();
    hb_buffer_add_utf8(buffer, text.data(), static_cast<int>(text.size()), 0, -1);
    hb_buffer_guess_segment_properties(buffer);
    hb_shape(m_hb_font, buffer, nullptr, 0);

    unsigned glyph_count;
    auto *glyph_infos = hb_buffer_get_glyph_infos(buffer, &glyph_count);
    auto *glyph_positions = hb_buffer_get_glyph_positions(buffer, &glyph_count);
    return {buffer, glyph_infos, glyph_positions, glyph_count};
}

const Optional<CachedGlyph> &Font::cached_glyph(uint32_t glyph_index) const {
    return m_glyph_cache[glyph_index];
}

} // namespace vull::ui
