#pragma once

#include <vull/maths/Vec.hh>
#include <vull/support/RingBuffer.hh>
#include <vull/support/Vector.hh>

#include <stdint.h>

namespace vull::ui {

class GpuFont;
class Renderer;

class TimeGraph {
public:
    // NOLINTNEXTLINE
    struct Section {
        const char *name;
        float duration{0.0f};
    };
    struct Bar {
        Vector<Section> sections;
    };

private:
    const Vec2f m_size;
    const Vec3f m_base_colour;
    const float m_bar_width;
    const float m_bar_spacing;
    RingBuffer<Bar> m_bars;
    Vector<Vec4f> m_section_colours;

    Vec4f colour_for_section(uint32_t section_index);

public:
    TimeGraph(const Vec2f &size, const Vec3f &base_colour, float bar_width = 3.0f, float bar_spacing = 1.0f);

    void add_bar(Bar &&bar);
    // TODO: Take in Optional<GpuFont &> and make font (legend rendering) and title rendering optional parameters.
    void draw(Renderer &renderer, const Vec2f &position, GpuFont &font, const char *title);
};

} // namespace vull::ui
