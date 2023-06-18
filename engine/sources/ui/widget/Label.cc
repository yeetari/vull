#include <vull/ui/widget/Label.hh>

#include <vull/support/Optional.hh>
#include <vull/support/String.hh>
#include <vull/support/Utility.hh>
#include <vull/ui/Element.hh>
#include <vull/ui/Font.hh>
#include <vull/ui/Painter.hh>
#include <vull/ui/Style.hh>
#include <vull/ui/Units.hh>

namespace vull::ui {

Label::Label(Tree &tree, Optional<Element &> parent) : Element(tree, parent), m_font(&style().main_font()) {}

Label::Label(Tree &tree, Optional<Element &> parent, String text) : Label(tree, parent) {
    set_text(vull::move(text));
}

void Label::recalculate_size() {
    if (!m_text.empty()) {
        auto bounds = m_font->text_bounds(m_text);
        set_minimum_size(bounds);
        set_maximum_size(bounds);
        set_computed_size(bounds);
    }
}

void Label::set_font(Font &font) {
    m_font = &font;
    recalculate_size();
}

void Label::set_text(String text) {
    m_text = vull::move(text);
    recalculate_size();
}

void Label::paint(Painter &painter, LayoutPoint position) const {
    if (m_text.empty()) {
        return;
    }
    painter.draw_text(*m_font, position + LayoutPoint(0, computed_height()), m_colour, m_text);
}

} // namespace vull::ui
