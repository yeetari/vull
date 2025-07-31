#include <vull/ui/units.hh>

#include <vull/ui/tree.hh>

namespace vull::ui {

LayoutUnit Length::resolve(Tree &tree, LayoutUnit maximum) const {
    switch (m_type) {
    case LengthType::Absolute:
        return m_layout_value;
    case LengthType::Cm:
        return LayoutUnit::from_float_pixels(m_float_value * tree.ppcm());
    case LengthType::Percentage:
        return maximum.scale_by(m_float_value / 100.0f);
    case LengthType::Grow:
        return maximum;
    default:
        return 0;
    }
}

LayoutSize Size::resolve(Tree &tree, LayoutSize maximum) const {
    return {m_width.resolve(tree, maximum.width()), m_height.resolve(tree, maximum.height())};
}

} // namespace vull::ui
