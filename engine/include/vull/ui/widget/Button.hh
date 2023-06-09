#pragma once

#include <vull/maths/Vec.hh>
#include <vull/support/Function.hh>
#include <vull/support/Optional.hh>
#include <vull/support/String.hh>
#include <vull/support/Utility.hh>
#include <vull/ui/Element.hh>
#include <vull/ui/widget/Label.hh>

namespace vull::ui {

class Painter;
class Font;
class MouseButtonEvent;
class Tree;

class Button : public Element {
    Label m_label;
    Function<void()> m_on_release;
    float m_padding{0.2f};

public:
    Button(Tree &tree, Optional<Element &> parent, Font &font, String text);

    void paint(Painter &painter, Vec2f position) const override;
    bool handle_mouse_press(const MouseButtonEvent &event) override;
    bool handle_mouse_release(const MouseButtonEvent &event) override;

    void set_text(String text);
    void set_on_release(Function<void()> on_release) { m_on_release = vull::move(on_release); }
};

} // namespace vull::ui
