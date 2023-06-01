#pragma once

#include <vull/container/HashMap.hh>
#include <vull/container/RingBuffer.hh>
#include <vull/container/Vector.hh>
#include <vull/maths/Colour.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Optional.hh>
#include <vull/support/String.hh>
#include <vull/ui/Element.hh>
#include <vull/ui/layout/BoxLayout.hh>

namespace vull::ui {

class CommandList;
class Font;
class Label;
class Pane;
class TimeGraph;
class Tree;

class TimeGraphPanel : public Element {
    TimeGraph &m_graph;

public:
    TimeGraphPanel(Tree &tree, Optional<Element &> parent, TimeGraph &graph) : Element(tree, parent), m_graph(graph) {}

    void paint(CommandList &cmd_list, Vec2f position) const override;
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
    Font &m_font;
    const String m_title;
    const float m_bar_width;

    Label *m_title_label;
    TimeGraphPanel *m_graph_panel;
    Pane *m_legend_vbox;

    RingBuffer<Bar> m_bars;
    Optional<Bar &> m_current_bar;
    float m_max_total_time{0.0f};

    HashMap<String, Colour> m_section_colours;
    Colour colour_for_section(const String &name);

public:
    TimeGraph(Tree &tree, Optional<Element &> parent, Vec2f size, const Colour &base_colour, Font &font, String title,
              float bar_width = 0.06f);

    void layout() override;
    void new_bar();
    void push_section(String name, float duration);
};

} // namespace vull::ui
