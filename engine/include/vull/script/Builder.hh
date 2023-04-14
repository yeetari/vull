#pragma once

#include <vull/container/Vector.hh>
#include <vull/script/Bytecode.hh>
#include <vull/script/Value.hh>
#include <vull/support/Optional.hh>
#include <vull/support/UniquePtr.hh>

#include <stdint.h>

namespace vull::script {

class ConstantPool;

enum class Op {
    None,

    // Binary arithmetic operators.
    Add,
    Sub,
    Mul,
    Div,

    // Unary operators.
    Negate,
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
    void emit_binary(Op op, Expr &lhs, Expr &rhs);
    void emit_return(Optional<Expr &> expr);
};

} // namespace vull::script
