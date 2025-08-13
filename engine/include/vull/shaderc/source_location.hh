#pragma once

#include <stdint.h>

namespace vull::shaderc {

class SourceLocation {
    uint64_t m_value;

public:
    explicit SourceLocation(uint64_t value) : m_value(value) {}
    SourceLocation(uint32_t byte_offset, uint32_t line)
        : m_value(static_cast<uint64_t>(byte_offset) | (static_cast<uint64_t>(line) << 32)) {}

    uint32_t byte_offset() const { return m_value & 0xffffffffu; }
    uint32_t line() const { return (m_value >> 32) & 0xffffffffu; }
};

} // namespace vull::shaderc
