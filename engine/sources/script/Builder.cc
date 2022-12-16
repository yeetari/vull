#include <vull/script/Builder.hh>

#include <vull/script/Bytecode.hh>
#include <vull/script/ConstantPool.hh>
#include <vull/script/Value.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Optional.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>

namespace vull::script {
namespace {

Number fold_arith(Op op, Number lhs, Number rhs) {
    switch (op) {
    case Op::Add:
        return lhs + rhs;
    case Op::Sub:
        return lhs - rhs;
    case Op::Mul:
        return lhs * rhs;
    case Op::Div:
        return lhs / rhs;
    default:
        vull::unreachable();
    }
}

bool fold(Op op, Expr &lhs, Expr &rhs) {
    if (lhs.kind != ExprKind::Number || rhs.kind != ExprKind::Number) {
        return false;
    }

    switch (op) {
    case Op::Add:
    case Op::Sub:
    case Op::Mul:
    case Op::Div:
        break;
    default:
        return false;
    }

    lhs.number_value = fold_arith(op, lhs.number_value, rhs.number_value);
    return true;
}

Instruction build(Opcode opcode, uint8_t a, uint8_t b, uint8_t c) {
    return Instruction(static_cast<uint32_t>(opcode) | static_cast<uint32_t>(a << 8u) |
                       static_cast<uint32_t>(b << 16u) | static_cast<uint32_t>(c << 24u));
}

} // namespace

void Builder::emit(Instruction inst) {
    m_insts.push(inst);
}

void Builder::emit(Opcode opcode, uint8_t a, uint8_t b, uint8_t c) {
    emit(build(opcode, a, b, c));
}

uint8_t Builder::materialise(Expr &expr) {
    if (expr.kind == ExprKind::Allocated) {
        // Already allocated.
        return expr.index;
    }
    if (expr.kind == ExprKind::Invalid) {
        // Ignore invalid expressions.
        return 0;
    }

    const auto reg = m_reg_count++;
    switch (expr.kind) {
    case ExprKind::Number: {
        const auto constant_index = m_constant_pool.put({expr.number_value});
        emit(Opcode::OP_loadk, reg, (constant_index >> 8u) & 0xffu, constant_index & 0xffu);
        break;
    }
    case ExprKind::Unallocated:
        expr.inst.set_a(reg);
        emit(expr.inst);
        break;
    default:
        VULL_ASSERT_NOT_REACHED("Unhandled ExprKind");
    }
    expr.kind = ExprKind::Allocated;
    return (expr.index = reg);
}

UniquePtr<Frame> Builder::build_frame() {
    VULL_ASSERT(!m_insts.empty());
    auto *memory = operator new(sizeof(Frame) + m_reg_count * sizeof(Value));
    return vull::adopt_unique(new (memory) Frame(m_insts.take_all()));
}

void Builder::emit_binary(Op op, Expr &lhs, Expr &rhs) {
    if (fold(op, lhs, rhs)) {
        return;
    }

    Opcode opcode{};
    switch (op) {
    case Op::Add:
        opcode = Opcode::OP_add;
        break;
    case Op::Sub:
        opcode = Opcode::OP_sub;
        break;
    case Op::Mul:
        opcode = Opcode::OP_mul;
        break;
    case Op::Div:
        opcode = Opcode::OP_div;
        break;
    default:
        VULL_ASSERT_NOT_REACHED();
    }
    lhs.inst = build(opcode, 0, materialise(lhs), materialise(rhs));
    lhs.kind = ExprKind::Unallocated;
}

void Builder::emit_return(Optional<Expr &> expr) {
    if (!expr) {
        emit(Opcode::OP_return0, 0, 0, 0);
        return;
    }
    emit(Opcode::OP_return1, materialise(*expr), 0, 0);
}

} // namespace vull::script
