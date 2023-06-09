#pragma once

#include <vull/container/Vector.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Optional.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/ui/Element.hh>

namespace vull::ui {

class Painter;

class Margins {
    Vec4f m_value;

public:
    void set(float top, float bottom, float left, float right) { m_value = {top, bottom, left, right}; }
    void set_all(float value) { m_value = {value}; }
    void set_top(float top) { m_value[0] = top; }
    void set_bottom(float bottom) { m_value[1] = bottom; }
    void set_left(float left) { m_value[2] = left; }
    void set_right(float right) { m_value[3] = right; }

    float top() const { return m_value[0]; }
    float bottom() const { return m_value[1]; }
    float left() const { return m_value[2]; }
    float right() const { return m_value[3]; }
};

class Pane : public Element {
    // TODO(small-vector)
    Vector<UniquePtr<Element>> m_children;
    Margins m_margins;

public:
    using Element::Element;

    bool is_pane() const override { return true; }

    template <typename T, typename... Args>
    T &add_child(Args &&...args);
    void clear_children();
    virtual void layout();

    Optional<HitResult> hit_test(Vec2f point) override;
    void paint(Painter &painter, Vec2f position) const override;

    const Vector<UniquePtr<Element>> &children() const { return m_children; }
    const Margins &margins() const { return m_margins; }
    Margins &margins() { return m_margins; }
};

template <typename T, typename... Args>
T &Pane::add_child(Args &&...args) {
    // NOLINTNEXTLINE
    auto *child = new T(tree(), *this, vull::forward<Args>(args)...);
    m_children.emplace(child);
    return *child;
}

} // namespace vull::ui
