#pragma once

class Shape;

class Collider {
    const Shape &m_shape;

public:
    explicit Collider(const Shape &shape) : m_shape(shape) {}

    const Shape &shape() const { return m_shape; }
};
