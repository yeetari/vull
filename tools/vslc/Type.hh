#pragma once

#include <stdint.h>

enum class ScalarType : uint8_t {
    Float,
    Uint,
};

class Type {
    ScalarType m_scalar_type;
    uint8_t m_vector_size : 4;
    uint8_t m_matrix_cols : 4;

public:
    Type() = default;
    Type(ScalarType scalar_type, uint8_t vector_size, uint8_t matrix_cols)
        : m_scalar_type(scalar_type), m_vector_size(vector_size), m_matrix_cols(matrix_cols) {}

    bool is_matrix() const { return m_matrix_cols > 1; }
    bool is_vector() const { return !is_matrix() && m_vector_size > 1; }
    bool is_scalar() const { return !is_matrix() && !is_vector(); }

    ScalarType scalar_type() const { return m_scalar_type; }
    uint8_t vector_size() const { return m_vector_size; }
    uint8_t matrix_cols() const { return m_matrix_cols; }
};
