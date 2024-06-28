#pragma once

#include <vull/container/hash_map.hh>
#include <vull/container/ring_buffer.hh>
#include <vull/container/vector.hh>
#include <vull/maths/colour.hh>
#include <vull/support/optional.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/ui/element.hh>
#include <vull/ui/layout/box_layout.hh>
#include <vull/ui/units.hh>

namespace vull::ui {

class Painter;
class Label;
class TimeGraph;
class Tree;

class TimeGraphPanel : public Element {
    TimeGraph &m_graph;
    mutable float m_max_total_time{0.0f};

public:
    TimeGraphPanel(Tree &tree, Optional<Element &> parent, TimeGraph &graph) : Element(tree, parent), m_graph(graph) {}

    void paint(Painter &painter, LayoutPoint position) const override;
    float max_total_time() const { return m_max_total_time; }
};

class TimeGraph : public VBoxLayout {
    friend TimeGraphPanel;

public:
    struct Section {
        String name;
        float duration{0.0f};
    };
    struct Bar {
        Vector<Section> sections;
    };

private:
    const Colour m_base_colour;
    const String m_title;
    Length m_bar_width{Length::zero()};

    Label *m_title_label;
    TimeGraphPanel *m_graph_panel;
    VBoxLayout *m_legend_vbox;

    RingBuffer<Bar> m_bars;
    Optional<Bar &> m_current_bar;

    HashMap<String, Colour> m_section_colours;
    Colour colour_for_section(const String &name);

public:
    TimeGraph(Tree &tree, Optional<Element &> parent, const Colour &base_colour, String title);

    void set_bar_width(Length bar_width);
    void pre_layout(LayoutSize available_space) override;
    void new_bar();
    void push_section(String name, float duration);
};

} // namespace vull::ui
