#include <vull/ui/widget/time_graph.hh>

#include <vull/container/hash_map.hh>
#include <vull/container/ring_buffer.hh>
#include <vull/container/vector.hh>
#include <vull/maths/colour.hh>
#include <vull/maths/common.hh>
#include <vull/support/algorithm.hh>
#include <vull/support/optional.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/utility.hh>
#include <vull/ui/element.hh>
#include <vull/ui/layout/box_layout.hh>
#include <vull/ui/painter.hh>
#include <vull/ui/style.hh>
#include <vull/ui/units.hh>
#include <vull/ui/widget/label.hh>

#include <stdint.h>

namespace vull::ui {

void TimeGraphPanel::paint(Painter &painter, LayoutPoint position) const {
    // Draw bounding box.
    painter.paint_rect(position, computed_size(), Colour::black());

    // Calculate visible bar count.
    const LayoutUnit bar_width = m_graph.m_bar_width.resolve(tree());
    const auto visible_bar_count =
        vull::min(static_cast<uint32_t>((computed_width() / bar_width).raw_value()) + 2, m_graph.m_bars.size());
    const auto bar_offset = m_graph.m_bars.size() - visible_bar_count;

    m_max_total_time = 0.0f;
    for (uint32_t bar_index = bar_offset; bar_index < m_graph.m_bars.size(); bar_index++) {
        float total_time = 0.0f;
        for (const auto &section : m_graph.m_bars[bar_index].sections) {
            total_time += section.duration;
        }
        m_max_total_time = vull::max(m_max_total_time, total_time);
    }

    // Draw bars.
    painter.set_scissor(position, computed_size());
    for (uint32_t bar_index = bar_offset; bar_index < m_graph.m_bars.size(); bar_index++) {
        const auto bar_base = position + LayoutDelta(bar_width * int32_t(bar_index - bar_offset), computed_height());
        for (LayoutUnit y_offset; const auto &section : m_graph.m_bars[bar_index].sections) {
            const auto &colour = m_graph.colour_for_section(section.name);
            auto height = computed_height().scale_by(section.duration / m_max_total_time);
            height = LayoutUnit::from_int_pixels(-height.round());
            painter.paint_rect(bar_base + LayoutDelta(0, y_offset), LayoutSize(bar_width, height), colour);
            y_offset += height;
        }
    }
    painter.unset_scissor();
}

TimeGraph::TimeGraph(Tree &tree, Optional<Element &> parent, const Colour &base_colour, String title)
    : VBoxLayout(tree, parent), m_base_colour(base_colour), m_title(vull::move(title)), m_bars(1000) {
    m_title_label = &add_child<Label>();
    auto &hbox = add_child<HBoxLayout>();
    m_graph_panel = &hbox.add_child<TimeGraphPanel>(*this);
    m_legend_vbox = &hbox.add_child<VBoxLayout>();
    m_legend_vbox->set_maximum_width(Length::shrink());
    set_bar_width(Length::make_cm(0.1f));
}

Colour TimeGraph::colour_for_section(const String &name) {
    if (!m_section_colours.contains(name)) {
        m_section_colours.set(name, vull::lerp(Colour::make_random(), m_base_colour, 0.55f));
    }
    return *m_section_colours.get(name);
}

void TimeGraph::set_bar_width(Length bar_width) {
    m_bar_width = bar_width;

    // TODO: Shouldn't need to resolve for this.
    const auto max_bar_count = static_cast<int32_t>(m_bars.size());
    LayoutUnit resolved_bar_width = m_bar_width.resolve(tree());
    m_graph_panel->set_minimum_width(Length::make_absolute(resolved_bar_width * 100));
    m_graph_panel->set_maximum_width(Length::make_absolute(resolved_bar_width * max_bar_count));
}

void TimeGraph::pre_layout(LayoutSize available_space) {
    auto title_string = vull::format("{}: {} ms", m_title, m_graph_panel->max_total_time() * 1000.0f);
    m_title_label->set_text(vull::move(title_string));

    m_legend_vbox->clear_children();
    const auto &latest_bar = m_bars[m_bars.size() - 1];
    for (const auto &section : vull::reverse_view(latest_bar.sections)) {
        const auto text = vull::format("{}: {} ms", section.name, section.duration * 1000.0f);
        auto &label = m_legend_vbox->add_child<Label>(vull::move(text));
        label.set_align(Align::Right);
        label.set_colour(colour_for_section(section.name));
        label.set_font(style().monospace_font());
    }
    VBoxLayout::pre_layout(available_space);
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
