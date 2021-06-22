#pragma once

#include <cstdint>

class Material {
    const std::uint32_t m_albedo_index;

public:
    explicit Material(std::uint32_t albedo_index) : m_albedo_index(albedo_index) {}

    std::uint32_t albedo_index() const { return m_albedo_index; }
};
