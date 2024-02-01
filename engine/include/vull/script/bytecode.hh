#pragma once

#include <vull/script/value.hh>
#include <vull/support/span.hh>

#include <stddef.h>
#include <stdint.h>

namespace vull::script {

enum class Opcode : uint8_t {
    OP_add,
    OP_sub,
    OP_mul,
    OP_div,
    OP_neg,

    OP_jmp,
    OP_iseq,
    OP_isne,
    OP_islt,
    OP_isle,

    OP_loadk,
    OP_move,
    OP_return0,
    OP_return1,
};

// a, b, c = 8-bit operands
// j = signed 24-bit operand
// o = opcode
// o = unused
// 1. cccccccc bbbbbbbb aaaaaaaa uuoooooo
// 2. jjjjjjjj jjjjjjjj jjjjjjjj uuoooooo
class Instruction {
    uint32_t m_word;

public:
    Instruction() = default;
    explicit Instruction(uint32_t word) : m_word(word) {}

    void set_a(uint8_t a) {
        m_word &= 0xffff00ffu;
        m_word |= static_cast<uint32_t>(a) << 8u;
    }

    void set_sj(int32_t offset) {
        m_word &= 0xffu;
        m_word |= static_cast<uint32_t>(offset) << 8u;
    }

    Opcode opcode() const { return static_cast<Opcode>(m_word & 0x3fu); }
    uint8_t a() const { return (m_word >> 8u) & 0xffu; }
    uint8_t b() const { return (m_word >> 16u) & 0xffu; }
    uint8_t c() const { return (m_word >> 24u) & 0xffu; }
    int32_t sj() const { return static_cast<int32_t>(m_word >> 8u); }
};

class Frame {
    friend class Vm;

private:
    Span<Instruction> m_insts;
    Instruction *m_ip;

public:
    explicit Frame(Span<Instruction> insts) : m_insts(insts), m_ip(insts.data()) {}
    Frame(const Frame &) = delete;
    Frame(Frame &&) = delete;
    ~Frame() { delete[] m_insts.data(); }

    Frame &operator=(const Frame &) = delete;
    Frame &operator=(Frame &&) = delete;

    Value &reg(uint8_t index) { return reinterpret_cast<Value *>(this + 1)[index]; }
};

} // namespace vull::script
