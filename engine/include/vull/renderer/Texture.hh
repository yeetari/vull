#pragma once

#include <vull/support/Vector.hh>

#include <cstdint>
#include <utility>

class Texture {
    std::uint32_t m_width{0};
    std::uint32_t m_height{0};
    Vector<std::uint8_t> m_data;

public:
    Texture(std::uint32_t width, std::uint32_t height, Vector<std::uint8_t> &&data)
        : m_width(width), m_height(height), m_data(std::move(data)) {}
    Texture(const Texture &) = delete;
    Texture(Texture &&) = delete;
    ~Texture() = default;

    Texture &operator=(const Texture &) = delete;
    Texture &operator=(Texture &&) = delete;

    std::uint32_t width() const { return m_width; }
    std::uint32_t height() const { return m_height; }
    const Vector<std::uint8_t> &data() const { return m_data; }
};
