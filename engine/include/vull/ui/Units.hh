#pragma once

#include <vull/maths/Vec.hh>

#include <stdint.h>

namespace vull::ui {

class Tree;

enum class Orientation : uint8_t {
    Horizontal,
    Vertical,
};

// 1 layout unit = 1/64 of a device (screen) pixel
class LayoutUnit {
    int32_t m_value{0};

public:
    static LayoutUnit from_float_pixels(float value) { return static_cast<int32_t>(value * 64.0f); }
    static LayoutUnit from_int_pixels(int32_t value) { return value * 64; }
    static LayoutUnit from_int_pixels(uint32_t value) { return static_cast<int32_t>(value) << 6; }

    LayoutUnit() = default;
    LayoutUnit(int32_t value) : m_value(value) {}

    LayoutUnit operator-() const { return -m_value; }
    LayoutUnit operator+(LayoutUnit rhs) const { return m_value + rhs.m_value; }
    LayoutUnit operator-(LayoutUnit rhs) const { return m_value - rhs.m_value; }
    LayoutUnit operator*(LayoutUnit rhs) const { return m_value * rhs.m_value; }
    LayoutUnit operator/(LayoutUnit rhs) const { return m_value / rhs.m_value; }

    bool operator<(LayoutUnit rhs) const { return m_value < rhs.m_value; }
    bool operator>(LayoutUnit rhs) const { return m_value > rhs.m_value; }
    bool operator==(LayoutUnit rhs) const { return m_value == rhs.m_value; }

    LayoutUnit &operator+=(LayoutUnit rhs) {
        m_value += rhs.m_value;
        return *this;
    }
    LayoutUnit &operator-=(LayoutUnit rhs) {
        m_value -= rhs.m_value;
        return *this;
    }

    LayoutUnit scale_by(float scale) const { return from_float_pixels(to_float() * scale); }

    int32_t fraction() const { return m_value % 64; }
    int32_t floor() const { return m_value >> 6; }
    int32_t round() const { return (m_value + 32) >> 6; }
    int32_t ceil() const {
        if (m_value >= 0) {
            return (m_value + 63) >> 6;
        }
        return m_value / 64;
    }

    int32_t raw_value() const { return m_value; }
    float to_float() const { return static_cast<float>(m_value) / 64.0f; }
};

template <typename T>
class LayoutVec {
    template <typename>
    friend class LayoutVec;

protected:
    LayoutUnit m_x;
    LayoutUnit m_y;

public:
    LayoutVec() = default;
    LayoutVec(LayoutUnit x, LayoutUnit y) : m_x(x), m_y(y) {}
    template <typename U>
    LayoutVec(LayoutVec<U> other) : m_x(other.m_x), m_y(other.m_y) {}

    template <typename U>
    T operator+(LayoutVec<U> rhs) const {
        return T(m_x + rhs.m_x, m_y + rhs.m_y);
    }
    template <typename U>
    T operator-(LayoutVec<U> rhs) const {
        return T(m_x - rhs.m_x, m_y - rhs.m_y);
    }
    template <typename U>
    T operator*(LayoutVec<U> rhs) const {
        return T(m_x * rhs.m_x, m_y * rhs.m_y);
    }
    template <typename U>
    T operator/(LayoutVec<U> rhs) const {
        return T(m_x / rhs.m_x, m_y / rhs.m_y);
    }
    T operator*(LayoutUnit rhs) const { return T(m_x * rhs, m_y * rhs); }
    T operator/(LayoutUnit rhs) const { return T(m_x / rhs, m_y / rhs); }

    template <typename U>
    T &operator+=(LayoutVec<U> rhs) {
        m_x += rhs.m_x;
        m_y += rhs.m_y;
        return static_cast<T &>(*this);
    }
    template <typename U>
    T &operator-=(LayoutVec<U> rhs) {
        m_x -= rhs.m_x;
        m_y -= rhs.m_y;
        return static_cast<T &>(*this);
    }

    Vec2i floor() const { return {m_x.floor(), m_y.floor()}; }
    Vec2i round() const { return {m_x.round(), m_y.round()}; }
    Vec2i ceil() const { return {m_x.ceil(), m_y.ceil()}; }
};

struct LayoutDelta : public LayoutVec<LayoutDelta> {
    static LayoutDelta from_int_pixels(Vec2i vec) {
        return {LayoutUnit::from_int_pixels(vec.x()), LayoutUnit::from_int_pixels(vec.y())};
    }

    LayoutDelta() = default;
    LayoutDelta(LayoutUnit dx, LayoutUnit dy) : LayoutVec(dx, dy) {}
    template <typename U>
    LayoutDelta(LayoutVec<U> other) : LayoutVec(other) {}

    void set_dx(LayoutUnit dx) { m_x = dx; }
    void set_dy(LayoutUnit dy) { m_y = dy; }
    LayoutUnit dx() const { return m_x; }
    LayoutUnit dy() const { return m_y; }
};

struct LayoutPoint : public LayoutVec<LayoutPoint> {
    using LayoutVec<LayoutPoint>::LayoutVec;

    void set_x(LayoutUnit x) { m_x = x; }
    void set_y(LayoutUnit y) { m_y = y; }
    LayoutUnit x() const { return m_x; }
    LayoutUnit y() const { return m_y; }
};

struct LayoutSize : public LayoutVec<LayoutSize> {
    static LayoutSize from_int_pixels(Vec2i vec) {
        return {LayoutUnit::from_int_pixels(vec.x()), LayoutUnit::from_int_pixels(vec.y())};
    }
    static LayoutSize from_int_pixels(Vec2u vec) {
        return {LayoutUnit::from_int_pixels(vec.x()), LayoutUnit::from_int_pixels(vec.y())};
    }

    LayoutSize() = default;
    LayoutSize(LayoutUnit width, LayoutUnit height) : LayoutVec(width, height) {}

    void set_width(LayoutUnit width) { m_x = width; }
    void set_height(LayoutUnit height) { m_y = height; }
    LayoutUnit width() const { return m_x; }
    LayoutUnit height() const { return m_y; }

    LayoutUnit cross_axis_length(Orientation orientation) const {
        return orientation == Orientation::Horizontal ? height() : width();
    }
    LayoutUnit main_axis_length(Orientation orientation) const {
        return orientation == Orientation::Horizontal ? width() : height();
    }
};

enum class LengthType : uint8_t {
    Absolute,
    Cm,
    Percentage,

    // Special values.
    Grow,
    Shrink,
};

class Length {
    union {
        float m_float_value{};
        LayoutUnit m_layout_value;
    };
    LengthType m_type;

    Length(LengthType type, float value) : m_float_value(value), m_type(type) {}
    Length(LengthType type, LayoutUnit value) : m_layout_value(value), m_type(type) {}

public:
    static Length make_absolute(LayoutUnit value) { return {LengthType::Absolute, value}; }
    static Length make_cm(float value) { return {LengthType::Cm, value}; }
    static Length make_percentage(float value) { return {LengthType::Percentage, value}; }
    static Length zero() { return make_absolute(LayoutUnit::from_int_pixels(0)); }
    static Length grow() { return {LengthType::Grow, 0.0f}; }
    static Length shrink() { return {LengthType::Shrink, 0.0f}; }

    LayoutUnit resolve(Tree &tree, LayoutUnit maximum = 0) const;
    bool is(LengthType type) const;
    bool is_one_of(auto... types) const;
};

class Size {
    Length m_width;
    Length m_height;

public:
    Size(LayoutSize absolute_size)
        : m_width(Length::make_absolute(absolute_size.width())),
          m_height(Length::make_absolute(absolute_size.height())) {}
    Size(Length width, Length height) : m_width(width), m_height(height) {}

    Length cross_axis_length(Orientation orientation) const {
        return orientation == Orientation::Vertical ? m_width : m_height;
    }
    Length main_axis_length(Orientation orientation) const {
        return orientation == Orientation::Vertical ? m_height : m_width;
    }

    LayoutSize resolve(Tree &tree, LayoutSize maximum) const;

    void set_width(Length width) { m_width = width; }
    void set_height(Length height) { m_height = height; }

    Length width() const { return m_width; }
    Length height() const { return m_height; }
};

inline bool Length::is(LengthType type) const {
    return m_type == type;
}

bool Length::is_one_of(auto... types) const {
    return (is(types) || ...);
}

} // namespace vull::ui
