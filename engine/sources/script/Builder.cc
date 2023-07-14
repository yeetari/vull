#include <vull/script/Builder.hh>

#include <vull/container/Vector.hh>
#include <vull/script/Bytecode.hh>
#include <vull/script/ConstantPool.hh>
#include <vull/script/Value.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Optional.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>

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

bool fold_unary(Op op, Expr &expr) {
    if (expr.kind != ExprKind::Number) {
        return false;
    }
    switch (op) {
    case Op::Negate:
        expr.number_value = -expr.number_value;
        return true;
    default:
        return false;
    }
}

bool fold_binary(Op op, Expr &lhs, Expr &rhs) {
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

void Builder::emit_unary(Op op, Expr &expr) {
    if (fold_unary(op, expr)) {
        return;
    }

    VULL_ASSERT(op == Op::Negate);
    expr.inst = build(Opcode::OP_neg, 0, materialise(expr), 0);
    expr.kind = ExprKind::Unallocated;
}

void Builder::emit_binary(Op op, Expr &lhs, Expr &rhs) {
    if (fold_binary(op, lhs, rhs)) {
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
    case Op::Equal:
        opcode = Opcode::OP_iseq;
        break;
    case Op::NotEqual:
        opcode = Opcode::OP_isne;
        break;
    case Op::GreaterThan:
        vull::swap(lhs, rhs);
        [[fallthrough]];
    case Op::LessThan:
        opcode = Opcode::OP_islt;
        break;
    case Op::GreaterEqual:
        vull::swap(lhs, rhs);
        [[fallthrough]];
    case Op::LessEqual:
        opcode = Opcode::OP_isle;
        break;
    default:
        VULL_ASSERT_NOT_REACHED();
    }

    if (opcode == Opcode::OP_iseq || opcode == Opcode::OP_islt || opcode == Opcode::OP_isle) {
        lhs.inst = build(opcode, materialise(lhs), materialise(rhs), 0);
        emit(lhs.inst);
    } else {
        lhs.inst = build(opcode, 0, materialise(lhs), materialise(rhs));
    }
    lhs.kind = ExprKind::Unallocated;
}

void Builder::emit_return(Optional<Expr &> expr) {
    if (!expr) {
        emit(Opcode::OP_return0, 0, 0, 0);
        return;
    }
    emit(Opcode::OP_return1, materialise(*expr), 0, 0);
}

uint32_t Builder::emit_jump() {
    emit(Opcode::OP_jmp, 0, 0, 0);
    return m_insts.size() - 1;
}

void Builder::patch_jump_to_here(uint32_t pc) {
    m_insts[pc].set_sj(static_cast<int32_t>(m_insts.size() - pc));
}

} // namespace vull::script
