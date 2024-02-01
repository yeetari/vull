#pragma once

#include <vull/support/function.hh>
#include <vull/support/optional.hh>
#include <vull/support/string.hh>
#include <vull/support/utility.hh>
#include <vull/ui/element.hh>
#include <vull/ui/units.hh>
#include <vull/ui/widget/label.hh>

namespace vull::ui {

class Painter;
class MouseButtonEvent;
class Tree;

class Button : public Element {
    Label m_label;
    Function<void()> m_on_release;
    Length m_padding{Length::make_cm(0.2f)};

public:
    Button(Tree &tree, Optional<Element &> parent, String text);

    void paint(Painter &painter, LayoutPoint position) const override;
    bool handle_mouse_press(const MouseButtonEvent &event) override;
    bool handle_mouse_release(const MouseButtonEvent &event) override;

    void set_text(String text);
    void set_on_release(Function<void()> on_release) { m_on_release = vull::move(on_release); }
};

} // namespace vull::ui
