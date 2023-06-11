#include <vull/ui/widget/Label.hh>

#include <vull/maths/Vec.hh>
#include <vull/support/Optional.hh>
#include <vull/support/String.hh>
#include <vull/support/Utility.hh>
#include <vull/ui/Element.hh>
#include <vull/ui/Font.hh>
#include <vull/ui/Painter.hh>
#include <vull/ui/Style.hh>
#include <vull/ui/Tree.hh>

namespace vull::ui {

Label::Label(Tree &tree, Optional<Element &> parent) : Element(tree, parent), m_font(&style().main_font()) {}

Label::Label(Tree &tree, Optional<Element &> parent, String text) : Label(tree, parent) {
    set_text(vull::move(text));
}

void Label::recalculate_size() {
    if (!m_text.empty()) {
        set_preferred_size(Vec2f(m_font->text_bounds(m_text)) / tree().global_scale());
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

void Label::paint(Painter &painter, Vec2f position) const {
    if (m_text.empty()) {
        return;
    }
    painter.draw_text(*m_font, position + Vec2f(0.0f, preferred_size().y()), m_colour, m_text);
}

} // namespace vull::ui
