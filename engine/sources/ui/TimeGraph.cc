#include <vull/ui/TimeGraph.hh>

#include <vull/maths/Common.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Algorithm.hh>
#include <vull/support/Array.hh>
#include <vull/support/RingBuffer.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/ui/Renderer.hh>

#include <stdio.h>
#include <stdlib.h>

namespace vull::ui {

TimeGraph::TimeGraph(const Vec2f &size, const Vec3f &base_colour, float bar_width, float bar_spacing)
    : m_size(size), m_base_colour(base_colour), m_bar_width(bar_width), m_bar_spacing(bar_spacing),
      m_bars(static_cast<uint32_t>(size.x() / (bar_width + bar_spacing))) {}

Vec4f TimeGraph::colour_for_section(uint32_t section_index) {
    if (m_section_colours.size() > section_index) {
        return m_section_colours[section_index];
    }
    m_section_colours.ensure_size(section_index + 1);
    auto rand_float = [] {
        return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    };
    Vec3f random_colour(rand_float(), rand_float(), rand_float());
    random_colour += m_base_colour;
    random_colour *= 0.5f;
    return (m_section_colours[section_index] = {random_colour.x(), random_colour.y(), random_colour.z(), 1.0f});
}

void TimeGraph::add_bar(Bar &&bar) {
    sort(bar.sections, [](const auto &lhs, const auto &rhs) {
        return lhs.duration < rhs.duration;
    });
    m_bars.enqueue(move(bar));
}

void TimeGraph::draw(Renderer &renderer, const Vec2f &position, GpuFont &font) {
    // Draw outline.
    renderer.draw_rect(Vec4f(1.0f), position, m_size + Vec2f(1.0f, 0.0f), false);

    // Calculate the max total time of all visible previous bars for scaling.
    float max_total_time = 0.0f;
    for (const auto &bar : m_bars) {
        float total_time = 0.0f;
        for (const auto &section : bar.sections) {
            total_time += section.duration;
        }
        max_total_time = max(max_total_time, total_time);
    }

    // Draw bars.
    for (uint32_t bar_index = 0; bar_index < m_bars.size(); bar_index++) {
        float x_offset = (m_bar_width + m_bar_spacing) * static_cast<float>(bar_index) + 1.0f;
        Vec2f bar_base = position + Vec2f(x_offset, m_size.y() - 1.0f);
        uint32_t section_index = 0;
        for (float y_offset = 0.0f; const auto &section : m_bars[bar_index].sections) {
            float height = section.duration / max_total_time * (m_size.y() - 2.0f);
            renderer.draw_rect(colour_for_section(section_index++), bar_base + Vec2f(0.0f, y_offset),
                               Vec2f(m_bar_width, -height));
            y_offset -= height;
        }
    }

    // Draw legend.
    const auto &latest_bar = m_bars[m_bars.size() - 1];
    uint32_t section_index = latest_bar.sections.size();
    for (float y_offset = position.y() + 10.0f; const auto &section : reverse_view(latest_bar.sections)) {
        Array<char, 256> buf{};
        // NOLINTNEXTLINE
        sprintf(buf.data(), "%s: %f ms", section.name, section.duration * 1000.0f);

        const auto &colour = colour_for_section(--section_index);
        renderer.draw_text(
            font, Vec3f(colour.x(), colour.y(), colour.z()),
            Vec2u(static_cast<uint32_t>(position.x() + m_size.x() + 10.0f), static_cast<uint32_t>(y_offset)),
            buf.data());
        y_offset += 30.0f;
    }
}

} // namespace vull::ui
