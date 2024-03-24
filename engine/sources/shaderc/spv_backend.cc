#include <vull/shaderc/spv_backend.hh>

#include <vull/container/hash_map.hh>
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

    Id id() const { return m_id; }
};

class AccessChain {
    Id m_base_id{};
    bool m_is_rvalue{false};

public:
    void set_lvalue(Id base_id) { m_base_id = base_id; }
    void set_rvalue(Id base_id) {
        m_base_id = base_id;
        m_is_rvalue = true;
    }

    Id base_id() const { return m_base_id; }
    bool is_rvalue() const { return m_is_rvalue; }
};

class Backend {
    Builder &m_builder;
    Id m_std_450{};

    Function *m_function{nullptr};
    Block *m_block{nullptr};
    HashMap<const hir::Expr *, Id> m_variable_map;

    AccessChain m_fragment_output_chain;
    bool m_is_fragment_entry{false};

private:
    Id lower_type(ScalarType scalar_type);
    Id lower_type(const Type &vsl_type);

    Id load_access_chain(const AccessChain &chain);
    void store_access_chain(const AccessChain &chain, Id rvalue);

    AccessChain lower_binary_expr(const hir::BinaryExpr *binary_expr);

    Tuple<Op, Id, Word> lower_builtin_function(hir::BuiltinFunction function) const;
    Value lower_builtin_expr(const hir::BuiltinExpr *builtin_expr);

    Value lower_constant(const hir::Constant *constant);

    Value lower_unary_expr(const hir::UnaryExpr *unary_expr);

    Id materialise_variable(const hir::Expr *expr);
    AccessChain lower_variable(const hir::Expr *expr);

    AccessChain lower_expr(const hir::Expr *expr);

    void lower_return_stmt(const hir::ReturnStmt *return_stmt);
    void lower_function_decl(const hir::FunctionDecl *function_decl);

public:
    explicit Backend(Builder &builder);

    void lower_top_level(const hir::Node *node);
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

Id Backend::load_access_chain(const AccessChain &chain) {
    VULL_ENSURE(!chain.is_rvalue());
    auto &load_inst = m_block->append(Op::Load, /*TODO*/ 0);
    load_inst.append_operand(chain.base_id());
    return load_inst.id();
}

void Backend::store_access_chain(const AccessChain &chain, Id rvalue) {
    // TODO: Optimise initial constant stores to variables.
    VULL_ASSERT(!chain.is_rvalue());
    auto &store_inst = m_block->append(Op::Store);
    store_inst.append_operand(chain.base_id());
    store_inst.append_operand(rvalue);
}

Op lower_binary_op(hir::BinaryOp op) {
    switch (op) {
        using enum hir::BinaryOp;
    case Add:
        return Op::FAdd;
    case Sub:
        return Op::FSub;
    case Div:
        return Op::FDiv;
    case ScalarTimesScalar:
        return Op::FMul;
    case VectorTimesScalar:
        return Op::VectorTimesScalar;
    case MatrixTimesScalar:
        return Op::MatrixTimesScalar;
    case VectorTimesMatrix:
        return Op::VectorTimesMatrix;
    case MatrixTimesVector:
        return Op::MatrixTimesVector;
    case MatrixTimesMatrix:
        return Op::MatrixTimesMatrix;
    }
}

AccessChain Backend::lower_binary_expr(const hir::BinaryExpr *binary_expr) {
    const auto lhs_chain = lower_expr(binary_expr->lhs());
    const auto rhs_chain = lower_expr(binary_expr->lhs());

    // Evaluate both sides as rvalues.
    Id lhs;
    if (binary_expr->op() != hir::BinaryOp::Assign) {
        lhs = load_access_chain(lhs_chain);
    }
    Id rhs = load_access_chain(rhs_chain);

    if (binary_expr->op() != hir::BinaryOp::Assign) {
        auto &inst = m_block->append(lower_binary_op(binary_expr->op()), lower_type(binary_expr->type()));
        inst.append_operand(lhs);
        inst.append_operand(rhs);
        rhs = inst.id();
    }

    if (binary_expr->is_assign()) {
        store_access_chain(lhs_chain, rhs);
        return lhs_chain;
    }

    // Non-assignment binary expressions create a new rvalue access chain.
    AccessChain access_chain;
    access_chain.set_rvalue(rhs);
    return access_chain;
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

Value Backend::lower_constant(const hir::Constant *constant) {
    auto &inst = m_builder.scalar_constant(lower_type(constant->type()), static_cast<Word>(constant->integer()));
    return {inst, constant->type()};
}

Value Backend::lower_unary_expr(const hir::UnaryExpr *unary_expr) {
    auto value = lower_expr(unary_expr->expr());
    auto &inst = m_block->append(Op::FNegate, lower_type(unary_expr->type()));
    inst.append_operand(value.id());
    return {inst, unary_expr->type()};
}

Id Backend::materialise_variable(const hir::Expr *expr) {
    if (expr->kind() == hir::NodeKind::LocalVar) {
        auto &variable = m_function->append_variable(lower_type(expr->type()));
        return variable.id();
    }

    if (expr->kind() == hir::NodeKind::PipelineVar) {
    }

    VULL_ENSURE_NOT_REACHED();
}

AccessChain Backend::lower_variable(const hir::Expr *expr) {
    Id id = m_variable_map.get(expr).value_or(0);
    if (id == 0) {
        id = materialise_variable(expr);
        m_variable_map.set(expr, id);
    }

    // Start a new lvalue access chain.
    AccessChain access_chain;
    access_chain.set_lvalue(id);
    return access_chain;
}

AccessChain Backend::lower_expr(const hir::Expr *expr) {
    switch (expr->kind()) {
    case hir::NodeKind::BinaryExpr:
        return lower_binary_expr(static_cast<const hir::BinaryExpr *>(expr));
    case hir::NodeKind::BuiltinExpr:
        return lower_builtin_expr(static_cast<const hir::BuiltinExpr *>(expr));
    case hir::NodeKind::Constant:
        return lower_constant(static_cast<const hir::Constant *>(expr));
    case hir::NodeKind::UnaryExpr:
        return lower_unary_expr(static_cast<const hir::UnaryExpr *>(expr));
    default:
        return lower_variable(expr);
    }
}

void Backend::lower_return_stmt(const hir::ReturnStmt *return_stmt) {
    Id rvalue = load_access_chain(lower_expr(return_stmt->expr()));
    if (m_is_fragment_entry) {
        store_access_chain(m_fragment_output_chain, rvalue);
        return;
    }
    auto &return_inst = m_block->append(Op::ReturnValue);
    return_inst.append_operand(rvalue);
}

void Backend::lower_function_decl(const hir::FunctionDecl *function_decl) {
    m_is_fragment_entry = function_decl->special_function() == hir::SpecialFunction::FragmentEntry;

    // Add implicit return if needed.
    if (!m_block->is_terminated()) {
        m_block->append(Op::Return);
    }
}

void Backend::lower_top_level(const hir::Node *node) {
    switch (node->kind()) {
    case hir::NodeKind::FunctionDecl:
        lower_function_decl(static_cast<const hir::FunctionDecl *>(node));
        break;
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

} // namespace

void build_spv(Builder &builder, const hir::Root &root) {
    Backend backend(builder);
    for (const auto *node : root.top_level_nodes()) {
        backend.lower_top_level(node);
    }
}

} // namespace vull::shaderc::spv
