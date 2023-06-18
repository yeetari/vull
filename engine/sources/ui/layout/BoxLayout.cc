#include <vull/ui/layout/BoxLayout.hh>

#include <vull/container/Vector.hh>
#include <vull/maths/Common.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/ui/Element.hh>
#include <vull/ui/Units.hh>
#include <vull/ui/layout/Pane.hh>

namespace vull::ui {

void BoxLayout::pre_layout(LayoutSize available_space) {
    const LayoutUnit available_main_axis_length = available_space.main_axis_length(m_orientation);
    const LayoutUnit spacing = m_spacing.resolve(tree(), available_main_axis_length);

    LayoutUnit main_axis = 0;
    LayoutUnit cross_axis = 0;
    for (const auto &child : children()) {
        Length child_main_axis = child->minimum_size().main_axis_length(m_orientation);
        main_axis += child_main_axis.resolve(tree(), available_main_axis_length);
        main_axis += spacing;

        Length child_cross_axis = child->minimum_size().cross_axis_length(m_orientation);
        cross_axis =
            vull::max(cross_axis, child_cross_axis.resolve(tree(), available_space.cross_axis_length(m_orientation)));

        if (child->is_pane()) {
            static_cast<Pane &>(*child).pre_layout({});
        }
    }

    switch (m_orientation) {
    case Orientation::Horizontal:
        main_axis += margins().left().resolve(tree()) + margins().right().resolve(tree());
        cross_axis += margins().top().resolve(tree()) + margins().bottom().resolve(tree());
        set_minimum_size({Length::make_absolute(main_axis), Length::make_absolute(cross_axis)});
        break;
    case Orientation::Vertical:
        cross_axis += margins().left().resolve(tree()) + margins().right().resolve(tree());
        main_axis += margins().top().resolve(tree()) + margins().bottom().resolve(tree());
        set_minimum_size({Length::make_absolute(cross_axis), Length::make_absolute(main_axis)});
        break;
    }
}

void BoxLayout::layout(LayoutSize available_space) {
    const LayoutUnit available_main_axis_length = available_space.main_axis_length(m_orientation);
    const LayoutUnit available_cross_axis_length = available_space.cross_axis_length(m_orientation);
    const LayoutUnit spacing = m_spacing.resolve(tree(), available_main_axis_length);

    LayoutUnit main_axis = margins().main_axis_start(m_orientation).resolve(tree());
    LayoutUnit cross_axis = margins().cross_axis_start(m_orientation).resolve(tree());

    // TODO: Helper function on Margins.
    LayoutUnit cross_axis_length = available_cross_axis_length;
    switch (m_orientation) {
    case Orientation::Horizontal:
        cross_axis_length -= margins().top().resolve(tree()) + margins().bottom().resolve(tree());
        break;
    case Orientation::Vertical:
        cross_axis_length -= margins().left().resolve(tree()) + margins().right().resolve(tree());
        break;
    }

    for (const auto &element : children()) {
        LayoutUnit child_cross_axis_length =
            element->maximum_size().cross_axis_length(m_orientation).resolve(tree(), cross_axis_length);

        LayoutUnit cross_axis_offset = 0;
        if (element->align() == Align::Center) {
            cross_axis_offset += cross_axis_length / 2 - child_cross_axis_length / 2;
        } else if (element->align() == Align::Right) {
            cross_axis_offset += cross_axis_length - child_cross_axis_length;
        }

        LayoutUnit child_main_axis_length = element->minimum_size().main_axis_length(m_orientation).resolve(tree());
        switch (m_orientation) {
        case Orientation::Horizontal:
            element->set_computed_size({child_main_axis_length, child_cross_axis_length});
            element->set_offset_in_parent({main_axis, cross_axis + cross_axis_offset});
            break;
        case Orientation::Vertical:
            element->set_computed_size({child_cross_axis_length, child_main_axis_length});
            element->set_offset_in_parent({cross_axis + cross_axis_offset, main_axis});
            break;
        }
        if (element->is_pane()) {
            static_cast<Pane &>(*element).layout(element->computed_size());
        }
        main_axis += child_main_axis_length + spacing;
    }

    switch (m_orientation) {
    case Orientation::Horizontal:
        set_computed_size({main_axis, available_cross_axis_length});
        break;
    case Orientation::Vertical:
        set_computed_size({available_cross_axis_length, main_axis});
        break;
    }
}

} // namespace vull::ui
