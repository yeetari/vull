#include <vull/ui/layout/box_layout.hh>

#include <vull/container/vector.hh>
#include <vull/maths/common.hh>
#include <vull/support/assert.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/ui/element.hh>
#include <vull/ui/layout/pane.hh>
#include <vull/ui/units.hh>

#include <stdint.h>

namespace vull::ui {
namespace {

struct LayoutItem {
    Element &element;
    LayoutUnit maximum_main_axis_length;
    LayoutUnit main_axis_length;
    LayoutUnit cross_axis_length;
    LayoutUnit cross_axis_offset;
    bool finalised{false};

    LayoutItem(Element &child) : element(child) {}
};

} // namespace

void BoxLayout::set_computed_main_axis(LayoutUnit length) {
    if (m_orientation == Orientation::Horizontal) {
        set_computed_width(length);
    } else {
        set_computed_height(length);
    }
}

void BoxLayout::set_computed_cross_axis(LayoutUnit length) {
    if (m_orientation == Orientation::Vertical) {
        set_computed_width(length);
    } else {
        set_computed_height(length);
    }
}

LayoutUnit BoxLayout::computed_main_axis() const {
    if (m_orientation == Orientation::Horizontal) {
        return computed_width();
    }
    return computed_height();
}

LayoutUnit BoxLayout::computed_cross_axis() const {
    if (m_orientation == Orientation::Vertical) {
        return computed_width();
    }
    return computed_height();
}

void BoxLayout::pre_layout(LayoutSize available_space) {
    const LayoutUnit available_main_axis_length = available_space.main_axis_length(m_orientation);
    const LayoutUnit spacing = m_spacing.resolve(tree(), available_main_axis_length);

    LayoutUnit main_axis = 0;
    LayoutUnit cross_axis = 0;
    for (const auto &child : children()) {
        if (!child->is_visible()) {
            continue;
        }
        if (child->is_pane()) {
            static_cast<Pane &>(*child).pre_layout({});
        }

        Length child_main_axis = child->minimum_size().main_axis_length(m_orientation);
        main_axis += child_main_axis.resolve(tree(), available_main_axis_length);
        main_axis += spacing;

        Length child_cross_axis = child->minimum_size().cross_axis_length(m_orientation);
        cross_axis =
            vull::max(cross_axis, child_cross_axis.resolve(tree(), available_space.cross_axis_length(m_orientation)));
    }

    main_axis += margins().main_axis_total(tree(), m_orientation);
    cross_axis += margins().cross_axis_total(tree(), m_orientation);
    switch (m_orientation) {
    case Orientation::Horizontal:
        set_minimum_size({Length::make_absolute(main_axis), Length::make_absolute(cross_axis)});
        break;
    case Orientation::Vertical:
        set_minimum_size({Length::make_absolute(cross_axis), Length::make_absolute(main_axis)});
        break;
    }
}

void BoxLayout::layout(LayoutSize available_space) {
    // Get total available main axis length and resolve the spacing property against it.
    const LayoutUnit available_main_axis_length = available_space.main_axis_length(m_orientation);
    const LayoutUnit spacing = m_spacing.resolve(tree(), available_main_axis_length);

    // Set computed cross axis length to the total available length.
    set_computed_cross_axis(available_space.cross_axis_length(m_orientation));

    // Create LayoutItems from child elements for processing.
    // TODO(small-vector)
    Vector<LayoutItem> items;
    items.ensure_capacity(children().size());
    for (const auto &child : children()) {
        if (child->is_visible()) {
            items.emplace(*child);
        }
    }

    if (items.empty()) {
        return;
    }

    // Calculate the maximum child cross axis length as the total available cross axis length minus the margins.
    // TODO: Percentage margins.
    LayoutUnit maximum_cross_axis_length = computed_cross_axis();
    maximum_cross_axis_length -= margins().cross_axis_total(tree(), m_orientation);

    // Calculate the cross axis length and offset for each item.
    for (auto &item : items) {
        // Resolve element maximum length against the box maximum length.
        item.cross_axis_length =
            item.element.maximum_size().cross_axis_length(m_orientation).resolve(tree(), maximum_cross_axis_length);

        if (item.element.align() == Align::Center) {
            item.cross_axis_offset = maximum_cross_axis_length / 2 - item.cross_axis_length / 2;
        } else if (item.element.align() == Align::Right) {
            item.cross_axis_offset = maximum_cross_axis_length - item.cross_axis_length;
        }
    }

    // Size all items to their minimum, keeping track of how much main axis space is leftover.
    LayoutUnit uncommitted_length = available_main_axis_length;
    uncommitted_length -= margins().main_axis_total(tree(), m_orientation);
    uncommitted_length -= spacing * int32_t(items.size() - 1);
    uint32_t unfinalised_item_count = items.size();
    for (auto &item : items) {
        // Begin item at minimum length.
        LayoutUnit minimum_length = item.element.minimum_size().main_axis_length(m_orientation).resolve(tree());
        item.main_axis_length = minimum_length;
        uncommitted_length -= minimum_length;

        // Calculate resolved maximum length, making sure to clamp to the minimum length in case shrink is used.
        LayoutUnit maximum_length =
            item.element.maximum_size().main_axis_length(m_orientation).resolve(tree(), available_main_axis_length);
        item.maximum_main_axis_length = vull::max(maximum_length, minimum_length);

        if (minimum_length == item.maximum_main_axis_length) {
            // Item has a fixed length.
            item.finalised = true;
            unfinalised_item_count--;
        }
    }

    // Share out the remaining length.
    while (uncommitted_length > 0 && unfinalised_item_count > 0) {
        LayoutUnit slice = uncommitted_length / int32_t(unfinalised_item_count);
        uncommitted_length = 0;
        for (auto &item : items) {
            if (item.finalised) {
                continue;
            }

            LayoutUnit resulting_length = item.main_axis_length + slice;
            item.main_axis_length = vull::min(resulting_length, item.maximum_main_axis_length);
            uncommitted_length += item.main_axis_length - resulting_length;

            VULL_ASSERT(item.main_axis_length <= item.maximum_main_axis_length);
            if (item.main_axis_length == item.maximum_main_axis_length) {
                item.finalised = true;
                unfinalised_item_count--;
            }
        }
    }

    // Place the items.
    LayoutUnit main_axis = margins().main_axis_start(tree(), m_orientation);
    LayoutUnit cross_axis = margins().cross_axis_start(tree(), m_orientation);
    for (const auto &item : items) {
        auto &element = item.element;
        if (m_orientation == Orientation::Horizontal) {
            element.set_computed_size({item.main_axis_length, item.cross_axis_length});
            element.set_offset_in_parent({main_axis, cross_axis + item.cross_axis_offset});
        } else {
            element.set_computed_size({item.cross_axis_length, item.main_axis_length});
            element.set_offset_in_parent({cross_axis + item.cross_axis_offset, main_axis});
        }
        if (element.is_pane()) {
            static_cast<Pane &>(element).layout(element.computed_size());
        }
        main_axis += item.main_axis_length + spacing;

        // Keep main axis offset rounded.
        main_axis = LayoutUnit::from_int_pixels(main_axis.round());
    }

    // Set computed main axis length.
    set_computed_main_axis(main_axis);
}

} // namespace vull::ui
