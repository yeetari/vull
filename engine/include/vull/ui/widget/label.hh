#pragma once

#include <vull/maths/colour.hh>
#include <vull/support/optional.hh>
#include <vull/support/string.hh>
#include <vull/ui/element.hh>
#include <vull/ui/units.hh> // IWYU pragma: keep

namespace vull::ui {

class Painter;
class Font;
class Tree;

class Label : public Element {
    Font *m_font;
    String m_text;
    Colour m_colour{Colour::white()};

    void recalculate_size();

public:
    Label(Tree &tree, Optional<Element &> parent);
    Label(Tree &tree, Optional<Element &> parent, String text);

    void set_colour(const Colour &colour) { m_colour = colour; }
    void set_font(Font &font);
    void set_text(String text);
    void paint(Painter &painter, LayoutPoint position) const override;
};

} // namespace vull::ui
