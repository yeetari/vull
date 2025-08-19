#pragma once

#include <stdint.h>

namespace vull::shaderc {

enum class ScalarType : uint8_t {
    Invalid,
    Float,
    Int,
    Uint,
    Void,
    Sampler,
};

enum class ImageType : uint8_t {
    Invalid,
    _2D,
    _3D,
    Cube,
};

class Type {
    ScalarType m_scalar_type{ScalarType::Invalid};
    ImageType m_image_type{ImageType::Invalid};
    uint8_t m_vector_size : 4 {0};
    uint8_t m_matrix_cols : 4 {0};
    bool m_has_sampler{false};

    Type(ScalarType scalar_type, ImageType image_type, uint8_t vector_size, uint8_t matrix_cols, bool has_sampler)
        : m_scalar_type(scalar_type), m_image_type(image_type), m_vector_size(vector_size), m_matrix_cols(matrix_cols),
          m_has_sampler(has_sampler) {}

public:
    static Type make_scalar(ScalarType scalar_type);
    static Type make_vector(ScalarType scalar_type, uint8_t size);
    static Type make_matrix(ScalarType scalar_type, uint8_t rows, uint8_t cols);
    static Type make_image(ScalarType scalar_type, ImageType image_type, bool has_sampler);

    Type() = default;

    bool is_valid() const { return m_scalar_type != ScalarType::Invalid; }
    bool is_image() const { return m_image_type != ImageType::Invalid; }
    bool is_matrix() const { return m_matrix_cols > 1; }
    bool is_vector() const { return !is_matrix() && m_vector_size > 1; }
    bool is_scalar() const { return !is_image() && !is_matrix() && !is_vector(); }

    ScalarType scalar_type() const { return m_scalar_type; }
    ImageType image_type() const { return m_image_type; }
    uint8_t vector_size() const { return m_vector_size; }
    uint8_t matrix_cols() const { return m_matrix_cols; }
    uint8_t matrix_rows() const { return m_vector_size; }
    bool has_sampler() const { return m_has_sampler; }
};

inline Type Type::make_scalar(ScalarType scalar_type) {
    return {scalar_type, ImageType::Invalid, 1, 1, false};
}

inline Type Type::make_vector(ScalarType scalar_type, uint8_t size) {
    return {scalar_type, ImageType::Invalid, size, 1, false};
}

inline Type Type::make_matrix(ScalarType scalar_type, uint8_t rows, uint8_t cols) {
    return {scalar_type, ImageType::Invalid, rows, cols, false};
}

inline Type Type::make_image(ScalarType scalar_type, ImageType image_type, bool has_sampler) {
    return {scalar_type, image_type, 0, 0, has_sampler};
}

constexpr bool operator==(const Type &lhs, const Type &rhs) {
    return lhs.scalar_type() == rhs.scalar_type() && lhs.image_type() == rhs.image_type() &&
           lhs.vector_size() == rhs.vector_size() && lhs.matrix_cols() == rhs.matrix_cols() &&
           lhs.has_sampler() == rhs.has_sampler();
}

} // namespace vull::shaderc
