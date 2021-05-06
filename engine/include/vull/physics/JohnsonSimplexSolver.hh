#pragma once

#include <vull/support/Array.hh>

#include <glm/vec3.hpp>

#include <cstdint>

class JohnsonSimplexSolver {
    std::uint8_t m_x_bits{0};
    std::uint8_t m_y_bits{0};
    std::uint8_t m_max_vertex_index{3};
    std::uint8_t m_last_vertex_index{3};
    Array<glm::vec3, 4> m_support_points{};
    Array<Array<glm::vec3, 4>, 4> m_edges{};
    Array<Array<float, 4>, 16> m_determinants{};

    std::uint8_t find_free_slot() const;

public:
    void add_point(const glm::vec3 &point, const glm::vec3 &support_point);
    bool is_proper_set(std::uint8_t set) const;
    bool is_simplex_point(std::uint8_t index) const;
    bool is_reduced_simplex_point(std::uint8_t index) const;
    bool is_valid_set(std::uint8_t set) const;
    bool reduce_simplex();
    void set_point(std::uint8_t index, const glm::vec3 &point, const glm::vec3 &support_point);
    void update_determinants(std::uint8_t index);
    void update_edges(std::uint8_t index);
    void update_max_vertex();

    glm::vec3 calculate_closest_point();
    glm::vec3 calculate_closest_point(std::uint8_t set) const;
    void calculate_backup_closest_point(glm::vec3 &result);

    bool is_empty_simplex() const;
    bool is_full_simplex() const;
    float max_vertex_sqrd() const;

    const glm::vec3 &point(std::uint8_t index) const;
    const glm::vec3 &support_point(std::uint8_t index) const;
};
