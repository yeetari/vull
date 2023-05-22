#include <vull/ui/Font.hh>

#include <vull/container/Array.hh>
#include <vull/container/FixedBuffer.hh>
#include <vull/container/Vector.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Result.hh>
#include <vull/support/ScopedLock.hh>
#include <vull/support/Span.hh>
#include <vull/support/StringView.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/tasklet/Mutex.hh>
#include <vull/vpak/FileSystem.hh>
#include <vull/vpak/PackFile.hh>
#include <vull/vpak/Reader.hh>

#include <ft2build.h> // IWYU pragma: keep
// IWYU pragma: no_include "freetype/config/ftheader.h"

#include <freetype/freetype.h>
#include <freetype/fterrors.h>
#include <freetype/ftimage.h>
#include <freetype/fttypes.h>
#include <hb-ft.h>
#include <hb.h>

namespace vull::ui {

ShapingPair ShapingIterator::operator*() const {
    const auto &position = m_glyph_positions[m_index];
    return ShapingPair{
        .glyph_index = m_glyph_infos[m_index].codepoint,
        .advance = {position.x_advance, position.y_advance},
        .offset = {position.x_offset, position.y_offset},
    };
}

ShapingView::~ShapingView() {
    hb_buffer_destroy(m_buffer);
}

Result<Font, FontLoadError> Font::load(StringView name, long size) {
    FT_Library library;
    if (FT_Init_FreeType(&library) != FT_Err_Ok) {
        return FontLoadError::FreetypeError;
    }

    auto entry = vpak::stat(name);
    auto stream = vpak::open(name);
    if (!entry || !stream) {
        return FontLoadError::NotFound;
    }

    auto bytes = ByteBuffer::create_uninitialised(entry->size);
    VULL_EXPECT(stream->read(bytes.span().as<uint8_t, uint32_t>()));

    FT_Face face;
    if (FT_New_Memory_Face(library, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &face) != FT_Err_Ok) {
        return FontLoadError::FreetypeError;
    }
    if (FT_Set_Char_Size(face, size * 64l, 0, 0, 0) != FT_Err_Ok) {
        return FontLoadError::FreetypeError;
    }
    return Font(library, vull::move(bytes), face);
}

Font::Font(FT_Library library, ByteBuffer &&bytes, FT_Face face)
    : m_library(library), m_bytes(vull::move(bytes)), m_hb_font(hb_ft_font_create_referenced(face)) {
    hb_ft_font_set_funcs(m_hb_font);
    m_glyph_cache.ensure_size(static_cast<uint32_t>(face->num_glyphs));
}

Font::~Font() {
    VULL_ASSERT(!m_mutex.locked());
    hb_font_destroy(m_hb_font);
    FT_Done_FreeType(m_library);
}

GlyphInfo Font::ensure_glyph(uint32_t glyph_index) const {
    ScopedLock lock(m_mutex);
    if (auto info = m_glyph_cache[glyph_index]) {
        return *info;
    }

    auto *face = hb_ft_font_get_face(m_hb_font);
    if (FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT) != FT_Err_Ok) {
        // TODO: What to do here?
        return {};
    }

    auto *glyph = face->glyph;
    return m_glyph_cache[glyph_index].emplace(GlyphInfo{
        .bitmap_extent = {glyph->bitmap.width, glyph->bitmap.rows},
        .bitmap_offset = {static_cast<float>(glyph->bitmap_left), -static_cast<float>(glyph->bitmap_top)},
    });
}

void Font::rasterise(uint32_t glyph_index, Span<uint8_t> buffer) const {
    ScopedLock lock(m_mutex);

    auto *face = hb_ft_font_get_face(m_hb_font);
    if (FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT) != FT_Err_Ok) {
        return;
    }

    auto *const glyph = face->glyph;
    if (FT_Render_Glyph(glyph, FT_RENDER_MODE_NORMAL) != FT_Err_Ok) {
        return;
    }

    const auto &bitmap = glyph->bitmap;
    for (unsigned y = 0; y < bitmap.rows; y++) {
        for (unsigned x = 0; x < bitmap.width; x++) {
            buffer[y * bitmap.width + x] = bitmap.buffer[int(y) * bitmap.pitch + int(x)];
        }
    }
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

} // namespace vull::ui
