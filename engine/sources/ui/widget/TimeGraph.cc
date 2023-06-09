#include <vull/ui/widget/TimeGraph.hh>

#include <vull/container/HashMap.hh>
#include <vull/container/RingBuffer.hh>
#include <vull/container/Vector.hh>
#include <vull/maths/Colour.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Algorithm.hh>
#include <vull/support/Format.hh>
#include <vull/support/Optional.hh>
#include <vull/support/String.hh>
#include <vull/support/Utility.hh>
#include <vull/ui/Painter.hh>
#include <vull/ui/layout/BoxLayout.hh>
#include <vull/ui/layout/Pane.hh>
#include <vull/ui/widget/Label.hh>

#include <stdint.h>

namespace vull::ui {

class Element;

void TimeGraphPanel::paint(Painter &painter, Vec2f position) const {
    // Draw bounding box.
    painter.draw_rect(position, preferred_size(), Colour::black());

    // Draw bars.
    for (uint32_t bar_index = 0; bar_index < m_graph.m_bars.size(); bar_index++) {
        float x_offset = m_graph.m_bar_width * static_cast<float>(bar_index);
        Vec2f bar_base = position + Vec2f(x_offset, preferred_size().y());
        for (float y_offset = 0.0f; const auto &section : m_graph.m_bars[bar_index].sections) {
            const auto &colour = m_graph.colour_for_section(section.name);
            float height = section.duration / m_graph.m_max_total_time * preferred_size().y();
            painter.draw_rect(bar_base + Vec2f(0.0f, y_offset), Vec2f(m_graph.m_bar_width, -height), colour);
            y_offset -= height;
        }
    }
}

// TODO: Don't take in size and bar_width
TimeGraph::TimeGraph(Tree &tree, Optional<Element &> parent, Vec2f size, const Colour &base_colour, Font &font,
                     String title, float bar_width)
    : VBoxLayout(tree, parent), m_base_colour(base_colour), m_font(font), m_title(vull::move(title)),
      m_bar_width(bar_width), m_bars(static_cast<uint32_t>(size.x() / m_bar_width)) {
    m_title_label = &add_child<Label>(font);
    auto &hbox = add_child<HBoxLayout>();
    m_graph_panel = &hbox.add_child<TimeGraphPanel>(*this);
    m_legend_vbox = &hbox.add_child<VBoxLayout>();

    m_graph_panel->set_preferred_size(size);
}

Colour TimeGraph::colour_for_section(const String &name) {
    if (!m_section_colours.contains(name)) {
        m_section_colours.set(name, vull::lerp(Colour::make_random(), m_base_colour, 0.55f));
    }
    return *m_section_colours.get(name);
}

void TimeGraph::layout() {
    m_max_total_time = 0.0f;
    for (const auto &bar : m_bars) {
        float total_time = 0.0f;
        for (const auto &section : bar.sections) {
            total_time += section.duration;
        }
        m_max_total_time = vull::max(m_max_total_time, total_time);
    }

    auto title_string = vull::format("{}: {} ms", m_title, m_max_total_time * 1000.0f);
    m_title_label->set_text(vull::move(title_string));

    m_legend_vbox->clear_children();
    const auto &latest_bar = m_bars[m_bars.size() - 1];
    for (const auto &section : vull::reverse_view(latest_bar.sections)) {
        const auto text = vull::format("{}: {} ms", section.name, section.duration * 1000.0f);
        auto &label = m_legend_vbox->add_child<Label>(m_font, vull::move(text));
        label.set_colour(colour_for_section(section.name));
    }
    VBoxLayout::layout();
}

void TimeGraph::new_bar() {
    m_current_bar = m_bars.emplace();
}

void TimeGraph::push_section(String name, float duration) {
    m_current_bar->sections.push({vull::move(name), duration});
    vull::sort(m_current_bar->sections, [](const auto &lhs, const auto &rhs) {
        return lhs.duration < rhs.duration;
    });
}

} // namespace vull::ui
