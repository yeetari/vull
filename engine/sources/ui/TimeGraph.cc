#include <vull/ui/TimeGraph.hh>

#include <vull/maths/Common.hh>
#include <vull/maths/Random.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Algorithm.hh>
#include <vull/support/Format.hh>
#include <vull/support/Optional.hh>
#include <vull/support/RingBuffer.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/ui/Renderer.hh>

namespace vull::ui {

TimeGraph::TimeGraph(const Vec2f &size, const Vec3f &base_colour, float bar_width, float bar_spacing)
    : m_size(size), m_base_colour(base_colour), m_bar_width(bar_width), m_bar_spacing(bar_spacing),
      m_bars(static_cast<uint32_t>(size.x() / (bar_width + bar_spacing))) {}

Vec4f TimeGraph::colour_for_section(uint32_t section_index) {
    if (m_section_colours.size() > section_index) {
        return m_section_colours[section_index];
    }
    m_section_colours.ensure_size(section_index + 1);
    auto random_colour = vull::linear_rand(Vec3f(0.1f), Vec3f(1.0f));
    random_colour += m_base_colour;
    random_colour *= 0.5f;
    return (m_section_colours[section_index] = {random_colour.x(), random_colour.y(), random_colour.z(), 1.0f});
}

void TimeGraph::add_bar(Bar &&bar) {
    vull::sort(bar.sections, [](const auto &lhs, const auto &rhs) {
        return lhs.duration < rhs.duration;
    });
    m_bars.enqueue(vull::move(bar));
}

void TimeGraph::draw(Renderer &renderer, const Vec2f &position, Optional<GpuFont &> font, StringView title) {
    // Draw bounding box.
    renderer.draw_rect(Vec4f(0.0f, 0.0f, 0.0f, 1.0f), position, m_size);

    // Calculate the max total time of all visible previous bars for scaling.
    float max_total_time = 0.0f;
    for (const auto &bar : m_bars) {
        float total_time = 0.0f;
        for (const auto &section : bar.sections) {
            total_time += section.duration;
        }
        max_total_time = vull::max(max_total_time, total_time);
    }

    // Draw bars.
    for (uint32_t bar_index = 0; bar_index < m_bars.size(); bar_index++) {
        float x_offset = (m_bar_width + m_bar_spacing) * static_cast<float>(bar_index);
        Vec2f bar_base = position + Vec2f(x_offset + m_bar_spacing, m_size.y());
        uint32_t section_index = 0;
        for (float y_offset = 0.0f; const auto &section : m_bars[bar_index].sections) {
            float height = section.duration / max_total_time * m_size.y();
            renderer.draw_rect(colour_for_section(section_index++), bar_base + Vec2f(0.0f, y_offset),
                               Vec2f(m_bar_width, -height));
            y_offset -= height;
        }
    }

    // Skip drawing title and legend if no font supplied.
    if (!font) {
        return;
    }

    if (!title.empty()) {
        auto title_string = vull::format("{}: {} ms", title, max_total_time * 1000.0f);
        renderer.draw_text(*font, Vec3f(1.0f), position - Vec2f(0.0f, 20.0f), title_string);
    }

    // Draw legend.
    const auto &latest_bar = m_bars[m_bars.size() - 1];
    uint32_t section_index = latest_bar.sections.size();
    for (float y_offset = position.y() + 10.0f; const auto &section : vull::reverse_view(latest_bar.sections)) {
        const auto &colour = colour_for_section(--section_index);
        const auto text = format("{}: {} ms", section.name, section.duration * 1000.0f);
        renderer.draw_text(*font, Vec3f(colour.x(), colour.y(), colour.z()),
                           Vec2f(position.x() + m_size.x() + 10.0f, y_offset), text);
        y_offset += 30.0f;
    }
}

} // namespace vull::ui
