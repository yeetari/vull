#include <vull/ui/layout/BoxLayout.hh>

#include <vull/container/Vector.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/ui/Element.hh>
#include <vull/ui/layout/Pane.hh>

namespace vull::ui {

void BoxLayout::layout() {
    Pane::layout();

    // TODO: Remove horrible ternaries with orientation.

    float cross_axis_size = 0.0f;
    for (const auto &child : children()) {
        cross_axis_size =
            vull::max(cross_axis_size, m_orientation == Orientation::Horizontal ? child->preferred_size().y()
                                                                                : child->preferred_size().x());
    }

    float x = margins().left();
    float y = margins().top();
    for (const auto &child : children()) {
        float cross_axis_offset = 0.0f;
        if ((child->flags() & ElementFlags::RightAlign) != ElementFlags::None) {
            cross_axis_offset +=
                cross_axis_size -
                (m_orientation == Orientation::Horizontal ? child->preferred_size().y() : child->preferred_size().x());
        }

        switch (m_orientation) {
        case Orientation::Horizontal:
            child->set_offset_in_parent({x, y + cross_axis_offset});
            x += child->preferred_size().x() + m_spacing;
            break;
        case Orientation::Vertical:
            child->set_offset_in_parent({x + cross_axis_offset, y});
            y += child->preferred_size().y() + m_spacing;
            break;
        }
    }

    switch (m_orientation) {
    case Orientation::Horizontal:
        set_preferred_size({x, cross_axis_size + margins().top() + margins().bottom()});
        break;
    case Orientation::Vertical:
        set_preferred_size({cross_axis_size + margins().left() + margins().right(), y});
        break;
    }
}

} // namespace vull::ui
