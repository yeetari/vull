#include <vull/shaderc/legaliser.hh>

#include <vull/container/hash_map.hh>
#include <vull/support/string_view.hh>

namespace vull::shaderc {

class Legaliser::Scope {
    Scope *&m_current;
    Scope *m_parent;
    HashMap<StringView, hir::Expr *> m_symbol_map;

public:
    explicit Scope(Scope *&current);
    Scope(const Scope &) = delete;
    Scope(Scope &&) = delete;
    ~Scope();

    Scope &operator=(const Scope &) = delete;
    Scope &operator=(Scope &&) = delete;

    Result<hir::Expr *, LegaliseError> lookup_symbol(StringView name) const;
    Result<void, LegaliseError> put_symbol(StringView name, hir::Expr *expr);
};

Legaliser::Scope::Scope(Scope *&current) : m_current(current), m_parent(current) {
    current = this;
}

Legaliser::Scope::~Scope() {
    m_current = m_parent;
}

Result<hir::Expr *, LegaliseError> Legaliser::Scope::lookup_symbol(StringView name) const {
    if (auto symbol = m_symbol_map.get(name)) {
        return *symbol;
    }
    if (m_parent == nullptr) {
        // TODO: Not found.
        VULL_ENSURE_NOT_REACHED();
    }
    return m_parent->lookup_symbol(name);
}

Result<void, LegaliseError> Legaliser::Scope::put_symbol(StringView name, hir::Expr *expr) {
    // TODO: Check for redeclaration.
    m_symbol_map.set(name, expr);
    return {};
}

Legaliser::Legaliser() : m_root_scope(new Scope(m_scope)) {}
Legaliser::~Legaliser() = default;

Result<hir::Expr *, LegaliseError> Legaliser::lower_binary_expr(const ast::BinaryExpr &ast_expr) {
    auto *lhs = VULL_TRY(lower_expr(ast_expr.lhs()));
    auto *rhs = VULL_TRY(lower_expr(ast_expr.rhs()));
    auto *expr = m_root.allocate<hir::BinaryExpr>(lhs, rhs);

    if (ast::is_assign_op(ast_expr.op())) {
        // TODO: Handle other assigns.
        VULL_ENSURE(ast_expr.op() == ast::BinaryOp::Assign);
        expr->set_op(hir::BinaryOp::Assign);
        expr->set_type(lhs->type());
        return expr;
    }

    const auto &lhs_type = lhs->type();
    const auto &rhs_type = rhs->type();
    if ((lhs_type.is_vector() && rhs_type.is_scalar()) || (lhs_type.is_scalar() && rhs_type.is_vector())) {
        expr->set_op(hir::BinaryOp::VectorTimesScalar);
        expr->set_type(lhs_type.is_vector() ? lhs_type : rhs_type);
    } else if ((lhs_type.is_matrix() && rhs_type.is_scalar()) || (lhs_type.is_scalar() && rhs_type.is_matrix())) {
        expr->set_op(hir::BinaryOp::MatrixTimesScalar);
        expr->set_type(lhs_type.is_matrix() ? lhs_type : rhs_type);
    } else if (lhs_type.is_vector() && rhs_type.is_matrix()) {
        expr->set_op(hir::BinaryOp::VectorTimesMatrix);
        expr->set_type(Type(lhs_type.scalar_type(), rhs_type.matrix_cols()));
    } else if (lhs_type.is_matrix() && rhs_type.is_vector()) {
        expr->set_op(hir::BinaryOp::MatrixTimesVector);
        expr->set_type(Type(lhs_type.scalar_type(), lhs_type.matrix_rows()));
    } else if (lhs_type.is_matrix() && rhs_type.is_matrix()) {
        expr->set_op(hir::BinaryOp::MatrixTimesMatrix);
        expr->set_type(Type(lhs_type.scalar_type(), rhs_type.matrix_cols(), lhs_type.matrix_rows()));
    } else if (lhs_type.is_scalar() && rhs_type.is_scalar()) {
        expr->set_op(hir::BinaryOp::ScalarTimesScalar);
        expr->set_type(lhs_type.scalar_type());
    } else {
        VULL_ENSURE_NOT_REACHED();
    }
    return expr;
}

Result<hir::Expr *, LegaliseError> Legaliser::lower_call_expr(const ast::CallExpr &ast_expr) {
    if (auto builtin = m_builtin_functions.get(ast_expr.name())) {
        // TODO: Need to check parameter types match.
        auto *expr = m_root.allocate<hir::BuiltinExpr>(vull::get<0>(*builtin));
        expr->set_type(vull::get<1>(*builtin));
        return expr;
    }
}

Result<hir::Expr *, LegaliseError> Legaliser::lower_symbol(const ast::Symbol &ast_symbol) {
    return VULL_TRY(m_scope->lookup_symbol(ast_symbol.name()));
}

Result<hir::Expr *, LegaliseError> Legaliser::lower_expr(const ast::Node &) {}

Result<hir::Node *, LegaliseError> Legaliser::lower_function_decl(const ast::FunctionDecl &ast_decl) {
    auto *function = m_root.allocate<hir::FunctionDecl>();

    return function;
}

Result<hir::Node *, LegaliseError> Legaliser::lower_pipeline_decl(const ast::PipelineDecl &ast_decl) {
    auto *expr = m_root.allocate<hir::Expr>(hir::NodeKind::PipelineVar);
    expr->set_type(ast_decl.type());
    VULL_TRY(m_scope->put_symbol(ast_decl.name(), expr));
    return nullptr;
}

Result<hir::Node *, LegaliseError> Legaliser::lower_top_level(const ast::Node &) {

}

Result<hir::Root, LegaliseError> Legaliser::legalise(const ast::Root &ast_root) {
    for (const auto *ast_node : ast_root.top_level_nodes()) {
        if (auto *node = VULL_TRY(lower_top_level(*ast_node))) {
            m_root.append_top_level(node);
        }
    }
    return vull::move(m_root);
}

} // namespace vull::shaderc
