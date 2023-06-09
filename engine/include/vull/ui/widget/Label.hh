#pragma once

#include <vull/maths/Colour.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Optional.hh>
#include <vull/support/String.hh>
#include <vull/support/Utility.hh>
#include <vull/ui/Element.hh>

namespace vull::ui {

class Painter;
class Font;
class Tree;

class Label : public Element {
    Font &m_font;
    String m_text;
    Colour m_colour{Colour::white()};

public:
    Label(Tree &tree, Optional<Element &> parent, Font &font) : Element(tree, parent), m_font(font) {}
    Label(Tree &tree, Optional<Element &> parent, Font &font, String text) : Element(tree, parent), m_font(font) {
        set_text(vull::move(text));
    }

    void set_colour(const Colour &colour) { m_colour = colour; }
    void set_text(String text);
    void paint(Painter &painter, Vec2f position) const override;
};

} // namespace vull::ui
