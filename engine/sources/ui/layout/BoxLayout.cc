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

    float x = margins().left();
    float y = margins().top();
    float width = 0.0f;
    float height = 0.0f;
    for (const auto &child : children()) {
        child->set_offset_in_parent({x, y});

        switch (m_orientation) {
        case Orientation::Horizontal:
            x += child->preferred_size().x() + m_spacing;
            height = vull::max(height, child->preferred_size().y());
            break;
        case Orientation::Vertical:
            y += child->preferred_size().y() + m_spacing;
            width = vull::max(width, child->preferred_size().x());
            break;
        }
    }

    switch (m_orientation) {
    case Orientation::Horizontal:
        set_preferred_size({x, height + margins().top() + margins().bottom()});
        break;
    case Orientation::Vertical:
        set_preferred_size({width + margins().left() + margins().right(), y});
        break;
    }
}

} // namespace vull::ui
