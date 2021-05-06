#include <vull/physics/JohnsonSimplexSolver.hh>

#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>

#include <glm/geometric.hpp>

#include <limits>

namespace {

bool is_subset_or_equal_to(std::uint8_t set, std::uint8_t subset) {
    return (set & subset) == subset;
}

} // namespace

std::uint8_t JohnsonSimplexSolver::find_free_slot() const {
    for (std::uint8_t i = 0; i < 4; i++) {
        if (!is_reduced_simplex_point(i)) {
            return i;
        }
    }
    return 4;
}

void JohnsonSimplexSolver::add_point(const glm::vec3 &point, const glm::vec3 &support_point) {
    std::uint8_t free_slot = find_free_slot();
    if (free_slot == 4) {
        return;
    }

    m_last_vertex_index = free_slot;
    m_y_bits = (1u << m_last_vertex_index) | m_x_bits;
    m_edges[m_last_vertex_index][m_last_vertex_index] = point;
    m_support_points[m_last_vertex_index] = support_point;

    update_edges(m_last_vertex_index);
    update_determinants(m_last_vertex_index);

    float point_sqrd = glm::dot(point, point);
    if (point_sqrd > max_vertex_sqrd()) {
        m_max_vertex_index = m_last_vertex_index;
    }
    m_determinants[0][m_last_vertex_index] = point_sqrd;
}

bool JohnsonSimplexSolver::is_proper_set(std::uint8_t set) const {
    for (std::uint8_t i = 0; i < 4; i++) {
        if ((set & (1u << i)) != 0 && m_determinants[set][i] <= 0) {
            return false;
        }
    }
    return true;
}

bool JohnsonSimplexSolver::is_simplex_point(std::uint8_t index) const {
    ASSERT(index < 4);
    return (m_y_bits & (1u << index)) > 0;
}

bool JohnsonSimplexSolver::is_reduced_simplex_point(std::uint8_t index) const {
    ASSERT(index < 4);
    return (m_x_bits & (1u << index)) > 0;
}

bool JohnsonSimplexSolver::is_valid_set(std::uint8_t set) const {
    for (std::uint8_t i = 0; i < 4; i++) {
        if (is_simplex_point(i)) {
            if ((set & (1u << i)) != 0) {
                if (m_determinants[set][i] <= 0) {
                    return false;
                }
            } else {
                if (m_determinants[set | (1u << i)][i] > 0) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool JohnsonSimplexSolver::reduce_simplex() {
    for (std::uint8_t subset = m_y_bits; subset > 0; subset--) {
        if ((subset & (1u << m_last_vertex_index)) != 0 && is_subset_or_equal_to(m_y_bits, subset) &&
            is_valid_set(subset)) {
            m_x_bits = subset;
            return true;
        }
    }
    return false;
}

void JohnsonSimplexSolver::set_point(std::uint8_t index, const glm::vec3 &point, const glm::vec3 &support_point) {
    ASSERT(index < 4);
    m_edges[index][index] = point;
    m_support_points[index] = support_point;
}

void JohnsonSimplexSolver::update_determinants(std::uint8_t index) {
    ASSERT(index < 4);
    std::uint8_t index_bit = 1u << index;
    m_determinants[index_bit][index] = 1;
    for (std::uint8_t i = 0; i < 4; i++) {
        if (i != index && is_reduced_simplex_point(i)) {
            std::uint8_t subset2 = (1u << i) | index_bit;
            m_determinants[subset2][i] = glm::dot(m_edges[index][i], point(index));
            m_determinants[subset2][index] = glm::dot(m_edges[i][index], point(i));
            for (std::uint8_t j = 0; j < i; j++) {
                if (j != index && is_reduced_simplex_point(j)) {
                    std::uint8_t subset3 = (1u << j) | subset2;
                    m_determinants[subset3][j] = glm::dot(m_edges[i][j], point(i)) * m_determinants[subset2][i] +
                                                 glm::dot(m_edges[i][j], point(index)) * m_determinants[subset2][index];

                    m_determinants[subset3][i] =
                        glm::dot(m_edges[j][i], point(j)) * m_determinants[(1u << j) | index_bit][j] +
                        glm::dot(m_edges[j][i], point(index)) * m_determinants[(1u << j) | index_bit][index];

                    m_determinants[subset3][index] =
                        glm::dot(m_edges[j][index], point(i)) * m_determinants[(1u << i) | (1u << j)][i] +
                        glm::dot(m_edges[j][index], point(j)) * m_determinants[(1u << i) | (1u << j)][j];
                }
            }
        }
    }

    if (m_y_bits != 0xf) {
        return;
    }

    m_determinants[m_y_bits][0] = dot(m_edges[1][0], point(1)) * m_determinants[0xe][1] +
                                  dot(m_edges[1][0], point(2)) * m_determinants[0xe][2] +
                                  dot(m_edges[1][0], point(3)) * m_determinants[0xe][3];

    m_determinants[m_y_bits][1] = dot(m_edges[0][1], point(0)) * m_determinants[0xd][0] +
                                  dot(m_edges[0][1], point(2)) * m_determinants[0xd][2] +
                                  dot(m_edges[0][1], point(3)) * m_determinants[0xd][3];

    m_determinants[m_y_bits][2] = dot(m_edges[0][2], point(0)) * m_determinants[0xb][0] +
                                  dot(m_edges[0][2], point(1)) * m_determinants[0xb][1] +
                                  dot(m_edges[0][2], point(3)) * m_determinants[0xb][3];

    m_determinants[m_y_bits][3] = dot(m_edges[0][3], point(0)) * m_determinants[0x7][0] +
                                  dot(m_edges[0][3], point(1)) * m_determinants[0x7][1] +
                                  dot(m_edges[0][3], point(2)) * m_determinants[0x7][2];
}

void JohnsonSimplexSolver::update_edges(std::uint8_t index) {
    ASSERT(index < 4);
    for (std::uint8_t i = 0; i < 4; i++) {
        if (i != index && is_simplex_point(i)) {
            m_edges[index][i] = m_edges[index][index] - m_edges[i][i];
            m_edges[i][index] = -m_edges[index][i];
        }
    }
}

void JohnsonSimplexSolver::update_max_vertex() {
    float max_vertex_sqrd = -std::numeric_limits<float>::max();
    for (std::uint8_t i = 0; i < 4; i++) {
        if (!is_simplex_point(i)) {
            continue;
        }
        float point_sqrd = glm::dot(m_edges[i][i], m_edges[i][i]);
        if (point_sqrd > max_vertex_sqrd) {
            max_vertex_sqrd = point_sqrd;
            m_max_vertex_index = i;
        }
    }
    m_determinants[0][m_max_vertex_index] = max_vertex_sqrd;
}

glm::vec3 JohnsonSimplexSolver::calculate_closest_point() {
    glm::vec3 closest_point(0.0f);
    float delta_x = 0.0f;
    float max_vertex_sqrd = 0.0f;
    for (std::uint8_t i = 0; i < 4; i++) {
        if (!is_reduced_simplex_point(i)) {
            continue;
        }
        float determinant = m_determinants[m_x_bits][i];
        glm::vec3 point = m_edges[i][i];
        closest_point += point * determinant;
        delta_x += determinant;
        if (m_determinants[0][i] > max_vertex_sqrd) {
            max_vertex_sqrd = m_determinants[0][i];
            m_max_vertex_index = i;
        }
    }
    return closest_point / delta_x;
}

glm::vec3 JohnsonSimplexSolver::calculate_closest_point(std::uint8_t set) const {
    glm::vec3 closest_point(0.0f);
    float delta_x = 0.0f;
    for (std::uint8_t i = 0; i < 4; i++) {
        if ((set & (1u << i)) == 0) {
            continue;
        }
        float determinant = m_determinants[set][i];
        glm::vec3 point = m_edges[i][i];
        closest_point += point * determinant;
        delta_x += determinant;
    }
    return closest_point / delta_x;
}

void JohnsonSimplexSolver::calculate_backup_closest_point(glm::vec3 &result) {
    float closest_point_sqrd = std::numeric_limits<float>::max();
    for (std::uint8_t subset = m_y_bits; subset > 0; subset--) {
        if (!is_subset_or_equal_to(m_y_bits, subset) || !is_proper_set(subset)) {
            continue;
        }
        glm::vec3 point = calculate_closest_point(subset);
        float point_sqrd = glm::dot(point, point);
        if (point_sqrd < closest_point_sqrd) {
            m_x_bits = subset;
            closest_point_sqrd = point_sqrd;
            result = point;
        }
    }
}

bool JohnsonSimplexSolver::is_empty_simplex() const {
    return m_x_bits == 0;
}

bool JohnsonSimplexSolver::is_full_simplex() const {
    return m_x_bits == 0xf;
}

float JohnsonSimplexSolver::max_vertex_sqrd() const {
    return m_determinants[0][m_max_vertex_index];
}

const glm::vec3 &JohnsonSimplexSolver::point(std::uint8_t index) const {
    ASSERT(index < 4);
    return m_edges[index][index];
}

const glm::vec3 &JohnsonSimplexSolver::support_point(std::uint8_t index) const {
    ASSERT(index < 4);
    return m_support_points[index];
}
