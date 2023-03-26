#pragma once

#include <vull/maths/Vec.hh>
#include <vull/support/HashMap.hh>
#include <vull/support/Optional.hh>
#include <vull/support/RingBuffer.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Vector.hh>

namespace vull::ui {

class CommandList;
class Font;

class TimeGraph {
public:
    struct Section {
        String name;
        float duration{0.0f};
    };
    struct Bar {
        Vector<Section> sections;
    };

private:
    const Vec2f m_size;
    const Vec3f m_base_colour;
    const float m_bar_width;
    RingBuffer<Bar> m_bars;
    HashMap<String, Vec4f> m_section_colours;
    Optional<Bar &> m_current_bar;

    Vec4f colour_for_section(const String &name);

public:
    TimeGraph(const Vec2f &size, const Vec3f &base_colour, float bar_width = 0.06f);

    void draw(CommandList &cmd_list, const Vec2f &position, Optional<Font &> font = {}, StringView title = {});
    void new_bar();
    void push_section(String name, float duration);
};

} // namespace vull::ui
