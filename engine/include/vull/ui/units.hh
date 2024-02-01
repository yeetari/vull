#pragma once

#include <vull/maths/common.hh>
#include <vull/maths/vec.hh>

#include <stdint.h>

namespace vull::ui {

class Tree;

enum class Orientation : uint8_t {
    Horizontal,
    Vertical,
};

/// A class for representing device (screen) pixels in subpixels for use in layout.
class LayoutUnit {
    int32_t m_value{0};

public:
    /**
     * Returns the smallest representable LayoutUnit as a fraction of a whole pixel.
     * @return a float representing a fractional pixel
     */
    static constexpr float epsilon() { return 1.0f / 64.0f; }

    /**
     * Returns the precision of a LayoutUnit, i.e.\ the number of layout units in a whole pixel.
     */
    static constexpr int32_t precision() { return 64; }

    /**
     * Creates a LayoutUnit from a rational number of pixels.
     * @param value floating-point number of pixels
     * @return      a LayoutUnit representing subpixels
     */
    static LayoutUnit from_float_pixels(float value) { return static_cast<int32_t>(value * 64.0f); }

    /**
     * Creates a LayoutUnit from an integer number of whole pixels.
     * @param value integer number of whole pixels
     * @return      a LayoutUnit representing subpixels
     */
    static LayoutUnit from_int_pixels(int32_t value) { return value * 64; }

    /**
     * @copydoc from_int_pixels(int32_t)
     */
    static LayoutUnit from_int_pixels(uint32_t value) { return static_cast<int32_t>(value) << 6; }

    LayoutUnit() = default;
    LayoutUnit(int32_t value) : m_value(value) {}

    LayoutUnit operator-() const { return -m_value; }
    LayoutUnit operator+(LayoutUnit rhs) const { return m_value + rhs.m_value; }
    LayoutUnit operator-(LayoutUnit rhs) const { return m_value - rhs.m_value; }
    LayoutUnit operator*(LayoutUnit rhs) const { return m_value * rhs.m_value; }
    LayoutUnit operator/(LayoutUnit rhs) const { return m_value / rhs.m_value; }
    LayoutUnit &operator+=(LayoutUnit rhs);
    LayoutUnit &operator-=(LayoutUnit rhs);
    LayoutUnit &operator*=(LayoutUnit rhs);
    LayoutUnit &operator/=(LayoutUnit rhs);

    bool operator<(LayoutUnit rhs) const { return m_value < rhs.m_value; }
    bool operator<=(LayoutUnit rhs) const { return m_value <= rhs.m_value; }
    bool operator>(LayoutUnit rhs) const { return m_value > rhs.m_value; }
    bool operator>=(LayoutUnit rhs) const { return m_value >= rhs.m_value; }
    bool operator==(LayoutUnit rhs) const { return m_value == rhs.m_value; }

    /**
     * Scales the layout unit by a rational ratio.
     * @param scale a floating-point ratio
     * @return      the scaled LayoutUnit
     */
    LayoutUnit scale_by(float scale) const;

    /**
     * Returns the fractional part of the layout unit.
     * @return an integer in the range [0, precision)
     * @see precision
     */
    int32_t fraction() const;

    /**
     * Rounds the layout unit down to the nearest whole pixel.
     * @return a whole number of integer pixels
     */
    int32_t floor() const;

    /**
     * Rounds the layout unit to the nearest whole pixel, with ties rounding away from zero.
     * @return a whole number of integer pixels
     */
    int32_t round() const;

    /**
     * Rounds the layout unit up to the nearest whole pixel.
     * @return a whole number of integer pixels
     */
    int32_t ceil() const;

    /**
     * Truncates the layout unit towards zero. Equivalent to floor when positive, and ceil when negative.
     * @return a whole number of integer pixels
     * @see floor
     * @see ceil
     */
    int32_t to_int() const { return m_value / 64; }

    /**
     * Converts the integer subpixels of the layout unit to a float.
     * @return a rational number of floating-point pixels
     */
    float to_float() const { return static_cast<float>(m_value) / 64.0f; }

    /**
     * Returns the raw value in integer subpixels.
     */
    int32_t raw_value() const { return m_value; }
};

template <typename T>
class LayoutVec {
    template <typename>
    friend class LayoutVec;

protected:
    LayoutUnit m_x;
    LayoutUnit m_y;

public:
    static T from_int_pixels(Vec2i vec) {
        return {LayoutUnit::from_int_pixels(vec.x()), LayoutUnit::from_int_pixels(vec.y())};
    }
    static T from_int_pixels(Vec2u vec) {
        return {LayoutUnit::from_int_pixels(vec.x()), LayoutUnit::from_int_pixels(vec.y())};
    }

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

inline LayoutUnit &LayoutUnit::operator+=(LayoutUnit rhs) {
    m_value += rhs.m_value;
    return *this;
}

inline LayoutUnit &LayoutUnit::operator-=(LayoutUnit rhs) {
    m_value -= rhs.m_value;
    return *this;
}

inline LayoutUnit &LayoutUnit::operator*=(LayoutUnit rhs) {
    m_value *= rhs.m_value;
    return *this;
}

inline LayoutUnit &LayoutUnit::operator/=(LayoutUnit rhs) {
    m_value /= rhs.m_value;
    return *this;
}

inline LayoutUnit LayoutUnit::scale_by(float scale) const {
    return from_float_pixels(to_float() * scale);
}

inline int32_t LayoutUnit::fraction() const {
    return m_value % 64;
}

inline int32_t LayoutUnit::floor() const {
    return m_value >> 6;
}

inline int32_t LayoutUnit::round() const {
    int32_t value = (vull::abs(m_value) + 32) >> 6;
    return m_value < 0 ? -value : value;
}

inline int32_t LayoutUnit::ceil() const {
    return (m_value + 63) >> 6;
}

inline bool Length::is(LengthType type) const {
    return m_type == type;
}

bool Length::is_one_of(auto... types) const {
    return (is(types) || ...);
}

} // namespace vull::ui
