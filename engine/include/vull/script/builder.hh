#pragma once

#include <vull/container/vector.hh>
#include <vull/script/bytecode.hh>
#include <vull/script/value.hh>
#include <vull/support/optional.hh>
#include <vull/support/unique_ptr.hh>

#include <stdint.h>

namespace vull::script {

class ConstantPool;

enum class Op : uint32_t {
    None = 0,

    // Binary arithmetic operators.
    Add,
    Sub,
    Mul,
    Div,

    // Binary comparison operators.
    Equal,
    NotEqual,
    LessThan,
    LessEqual,
    GreaterThan,
    GreaterEqual,

    // Unary operators.
    Negate,

    _count,
};

enum class ExprKind {
    Allocated,
    Constant,
    Invalid,
    Number,
    Unallocated,
};

struct Expr {
    ExprKind kind{ExprKind::Invalid};
    union {
        uint8_t index;       // For Allocated (register index), Constant (constant index)
        Instruction inst;    // For Unallocated
        Number number_value; // For Number
    };
};

class Builder {
    ConstantPool &m_constant_pool;
    Vector<Instruction> m_insts;
    uint8_t m_reg_count{0};

    void emit(Instruction inst);
    void emit(Opcode opcode, uint8_t a, uint8_t b, uint8_t c);

public:
    explicit Builder(ConstantPool &constant_pool) : m_constant_pool(constant_pool) {}

    uint8_t materialise(Expr &expr);
    UniquePtr<Frame> build_frame();
    void emit_unary(Op op, Expr &expr);
    void emit_binary(Op op, Expr &lhs, Expr &rhs);
    void emit_return(Optional<Expr &> expr);

    uint32_t emit_jump();
    void patch_jump_to_here(uint32_t pc);
};

} // namespace vull::script
