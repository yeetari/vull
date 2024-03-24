#include <vull/shaderc/spv_backend.hh>

#include <vull/shaderc/hir.hh>
#include <vull/shaderc/spv_builder.hh>
#include <vull/support/tuple.hh>

namespace vull::shaderc::spv {
namespace {

// Extend from Type to allow for nicer access to type-related functions.
class Value : public Type {
    Id m_id;
    Op m_creator_op{};

public:
    Value(Id id, Type type) : Type(type), m_id(id) {}
    Value(const Instruction &inst, Type type) : Type(type), m_id(inst.id()), m_creator_op(inst.op()) {}
};

class Backend {
    Builder &m_builder;
    Id m_std_450{};

    Block *m_block{nullptr};

public:
    explicit Backend(Builder &builder);

    Id lower_type(ScalarType scalar_type);
    Id lower_type(const Type &vsl_type);

    Value lower_binary_expr(const hir::BinaryExpr *binary_expr);

    Tuple<Op, Id, Word> lower_builtin_function(hir::BuiltinFunction function) const;
    Value lower_builtin_expr(const hir::BuiltinExpr *builtin_expr);

    Value lower_expr(const hir::Expr *expr);

    void lower_function_decl(const hir::FunctionDecl *function_decl);
};

Backend::Backend(Builder &builder) : m_builder(builder) {
    m_std_450 = builder.import_extension("GLSL.std.450");
}

Id Backend::lower_type(ScalarType scalar_type) {
    switch (scalar_type) {
    case ScalarType::Void:
        return m_builder.void_type();
    case ScalarType::Float:
        return m_builder.float_type(32);
    case ScalarType::Int:
        return m_builder.int_type(32, true);
    case ScalarType::Uint:
        return m_builder.int_type(32, false);
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

Id Backend::lower_type(const Type &vsl_type) {
    const auto scalar_type = lower_type(vsl_type.scalar_type());
    if (vsl_type.is_scalar()) {
        return scalar_type;
    }
    const auto vector_type = m_builder.vector_type(scalar_type, vsl_type.vector_size());
    if (vsl_type.is_vector()) {
        return vector_type;
    }
    return m_builder.matrix_type(vector_type, vsl_type.matrix_cols());
}

Value Backend::lower_binary_expr(const hir::BinaryExpr *binary_expr) {

}

Tuple<Op, Id, Word> Backend::lower_builtin_function(hir::BuiltinFunction function) const {
    switch (function) {
        using enum hir::BuiltinFunction;
    case Dot:
        return vull::make_tuple(Op::Dot, 0, 0);
    case Max:
        return vull::make_tuple(Op::ExtInst, m_std_450, 40);
    }
}

Value Backend::lower_builtin_expr(const hir::BuiltinExpr *builtin_expr) {
    // TODO
    Vector<Id> argument_ids;

    const auto [op, set_id, set_inst] = lower_builtin_function(builtin_expr->function());
    auto &inst = m_block->append(op, lower_type(builtin_expr->type()));
    if (op == Op::ExtInst) {
        inst.append_operand(set_id);
        inst.append_operand(set_inst);
    }
    inst.extend_operands(argument_ids);
    return {inst, builtin_expr->type()};
}

Value Backend::lower_expr(const hir::Expr *expr) {
    switch (expr->kind()) {
    case hir::NodeKind::BinaryExpr:
        return lower_binary_expr(static_cast<const hir::BinaryExpr *>(expr));
    case hir::NodeKind::BuiltinExpr:
        return lower_builtin_expr(static_cast<const hir::BuiltinExpr *>(expr));

    }
}

void Backend::lower_function_decl(const hir::FunctionDecl *function_decl) {

    // Add implicit return if needed.
    if (!m_block->is_terminated()) {
        m_block->append(Op::Return);
    }
}

} // namespace

void build_spv(Builder &builder, const hir::Root &hir_root) {
    Backend backend(builder);
}

} // namespace vull::shaderc::spv
