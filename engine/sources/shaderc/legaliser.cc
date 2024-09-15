#include "vull/shaderc/ast.hh"
#include <vull/shaderc/legaliser.hh>

#include <vull/shaderc/hir.hh>
#include <vull/support/assert.hh>

// The legaliser turns the parsed AST into a high-level IR (HIR) which has a similar overall structure to the AST but
// with the following changes:
// 1) Symbols and lexical scoping in general is removed
// 2) All expressions become typed
// 3) Some higher level binary operations turn into more specific operations, e.g. multiplication could turn into
//    MatrixTimesVector
// 4) ast::CallExpr is lowered into either hir::CallExpr or hir::BuiltinFunction

namespace vull::shaderc {

class Legaliser::Scope {
    Scope *&m_current;
    Scope *m_parent;
    HashMap<StringView, hir::NodeHandle<hir::Expr>> m_symbol_map;

public:
    explicit Scope(Scope *&current);
    Scope(const Scope &) = delete;
    Scope(Scope &&) = delete;
    ~Scope();

    Scope &operator=(const Scope &) = delete;
    Scope &operator=(Scope &&) = delete;

    LegaliseResult<hir::Expr> lookup_symbol(StringView name) const;
    Result<void, LegaliseError> put_symbol(StringView, hir::NodeHandle<hir::Expr> &&expr);
};

Legaliser::Scope::Scope(Scope *&current) : m_current(current), m_parent(current) {
    current = this;
}

Legaliser::Scope::~Scope() {
    m_current = m_parent;
}

LegaliseResult<hir::Expr> Legaliser::Scope::lookup_symbol(StringView name) const {
    if (auto symbol = m_symbol_map.get(name)) {
        return symbol->share();
    }
    if (m_parent == nullptr) {
        // TODO
        VULL_ENSURE_NOT_REACHED();
    }
    return m_parent->lookup_symbol(name);
}

Result<void, LegaliseError> Legaliser::Scope::put_symbol(StringView name, hir::NodeHandle<hir::Expr> &&expr) {
    // TODO: Check for redeclaration.
    m_symbol_map.set(name, vull::move(expr));
    return {};
}

Legaliser::Legaliser(hir::Root &root) : m_root(root), m_root_scope(new Scope(m_scope)) {
    // m_builtin_functions.set("dot", hir::BuiltinFunction::Dot);
}

Legaliser::~Legaliser() = default;

LegaliseResult<hir::Expr> Legaliser::lower_binary_expr(const ast::BinaryExpr &ast_expr) {
    auto expr = m_root.allocate<hir::BinaryExpr>();
    expr->set_lhs(VULL_TRY(lower_expr(ast_expr.lhs())));
    expr->set_rhs(VULL_TRY(lower_expr(ast_expr.rhs())));

    // Handle assignments.
    const auto lhs_type = expr->lhs().type();
    if (ast::is_assign_op(ast_expr.op())) {
        // TODO: Handle other assigns.
        VULL_ENSURE(ast_expr.op() == ast::BinaryOp::Assign);
        expr->set_op(hir::BinaryOp::Assign);
        expr->set_is_assign(true);

        // Result of assign expression is the modified variable.
        expr->set_type(lhs_type);
        return vull::move(expr);
    }

    const auto rhs_type = expr->rhs().type();

    // Specialise multiplication operator.
    if (ast_expr.op() == ast::BinaryOp::Mul) {
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
        } else if (lhs_type.is_vector() && rhs_type.is_vector()) {
            expr->set_op(hir::BinaryOp::VectorTimesVector);
            expr->set_type(lhs_type);
        } else {
            // Shouldn't be possible.
            VULL_ENSURE_NOT_REACHED();
        }
        return expr;
    }

    // Otherwise just forward the AST operator.
    expr->set_op([](ast::BinaryOp op) {
        switch (op) {
            using enum ast::BinaryOp;
        case Add:
            return hir::BinaryOp::Add;
        case Sub:
            return hir::BinaryOp::Sub;
        case Div:
            return hir::BinaryOp::Div;
        case Mod:
            return hir::BinaryOp::Mod;
        default:
            return hir::BinaryOp::Invalid;
        }
    }(ast_expr.op()));

    // TODO: Proper type checking.
    expr->set_type(expr->lhs().type());
    return expr;
}

LegaliseResult<hir::Expr> Legaliser::lower_call_expr(const ast::CallExpr &ast_expr) {
    if (auto builtin = m_builtin_functions.get(ast_expr.name())) {
        // TODO: Need to check parameter types match.
        auto expr = m_root.allocate<hir::BuiltinExpr>(vull::get<0>(*builtin));
        expr->set_type(vull::get<1>(*builtin));
        return expr;
    }
    VULL_ENSURE_NOT_REACHED();
}

LegaliseResult<hir::Expr> Legaliser::lower_constant(const ast::Constant &ast_constant) {
    return m_root.allocate<hir::Constant>(ast_constant.integer(), ast_constant.scalar_type());
}

LegaliseResult<hir::Expr> Legaliser::lower_construct_expr(const ast::Aggregate &ast_expr) {
    VULL_ASSERT(ast_expr.kind() == ast::AggregateKind::ConstructExpr);
    auto expr = m_root.allocate<hir::ConstructExpr>();
    expr->set_type(ast_expr.type());
    for (const auto &value : ast_expr.nodes()) {
        expr->append_value(VULL_TRY(lower_expr(*value)));
    }
    return expr;
}

LegaliseResult<hir::Expr> Legaliser::lower_symbol(const ast::Symbol &ast_symbol) {
    return VULL_TRY(m_scope->lookup_symbol(ast_symbol.name()));
}

LegaliseResult<hir::Expr> Legaliser::lower_expr(const ast::Node &ast_expr) {
    switch (ast_expr.kind()) {
    case ast::NodeKind::Aggregate:
        return VULL_TRY(lower_construct_expr(static_cast<const ast::Aggregate &>(ast_expr)));
    case ast::NodeKind::BinaryExpr:
        return VULL_TRY(lower_binary_expr(static_cast<const ast::BinaryExpr &>(ast_expr)));
    case ast::NodeKind::Constant:
        return VULL_TRY(lower_constant(static_cast<const ast::Constant &>(ast_expr)));
    case ast::NodeKind::Symbol:
        return VULL_TRY(lower_symbol(static_cast<const ast::Symbol &>(ast_expr)));
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

LegaliseResult<hir::Node> Legaliser::lower_decl_stmt(const ast::DeclStmt &ast_stmt) {
    auto variable = m_root.allocate<hir::Expr>(hir::NodeKind::LocalVar);
    variable->set_type(ast_stmt.value().type());

    // Generate assign for initialiser.
    auto initialiser = VULL_TRY(lower_expr(ast_stmt.value()));
    auto assign = m_root.allocate<hir::BinaryExpr>();
    assign->set_lhs(variable.share());
    assign->set_rhs(vull::move(initialiser));
    assign->set_op(hir::BinaryOp::Assign);
    assign->set_is_assign(true);

    VULL_TRY(m_scope->put_symbol(ast_stmt.name(), vull::move(variable)));
    return m_root.allocate<hir::ExprStmt>(vull::move(assign));
}

LegaliseResult<hir::Node> Legaliser::lower_return_stmt(const ast::ReturnStmt &ast_stmt) {
    auto expr = VULL_TRY(lower_expr(ast_stmt.expr()));
    VULL_ENSURE_NOT_REACHED();
}

LegaliseResult<hir::Node> Legaliser::lower_stmt(const ast::Node &ast_stmt) {
    switch (ast_stmt.kind()) {
    case ast::NodeKind::DeclStmt:
        return VULL_TRY(lower_decl_stmt(static_cast<const ast::DeclStmt &>(ast_stmt)));
    case ast::NodeKind::ReturnStmt:
        return VULL_TRY(lower_return_stmt(static_cast<const ast::ReturnStmt &>(ast_stmt)));
    default:
        // Expression statement.
        return m_root.allocate<hir::ExprStmt>(VULL_TRY(lower_expr(ast_stmt)));
    }
}

LegaliseResult<hir::Aggregate> Legaliser::lower_block(const ast::Aggregate &ast_block) {
    VULL_ASSERT(ast_block.kind() == ast::AggregateKind::Block);
    auto block = m_root.allocate<hir::Aggregate>(hir::NodeKind::Block);
    for (const auto &ast_stmt : ast_block.nodes()) {
        block->append_node(VULL_TRY(lower_stmt(*ast_stmt)));
    }
    return block;
}

LegaliseResult<hir::FunctionDecl> Legaliser::lower_function_decl(const ast::FunctionDecl &ast_decl) {
    auto function = m_root.allocate<hir::FunctionDecl>();

    // Assign any special function.
    if (ast_decl.name() == "vertex_main") {
        function->set_special_function(hir::SpecialFunction::VertexEntry);
    } else if (ast_decl.name() == "fragment_main") {
        function->set_special_function(hir::SpecialFunction::FragmentEntry);
    }

    // Create new scope and create argument values from parameters.
    Scope scope(m_scope);
    for (uint32_t i = 0; i < ast_decl.parameters().size(); i++) {
        hir::NodeHandle<hir::Expr> argument;
        if (function->is_special_function(hir::SpecialFunction::VertexEntry)) {
            // Parameter is actually a vertex input location.
            argument = m_root.allocate<hir::PipelineVariable>(i, false);
        } else {
            argument = m_root.allocate<hir::Expr>(hir::NodeKind::Argument);
        }

        const auto &parameter = ast_decl.parameters()[i];
        argument->set_type(parameter.type());
        VULL_TRY(scope.put_symbol(parameter.name(), vull::move(argument)));
    }

    auto create_pipeline_variable =
        [&](StringView name, Type type,
            Variant<uint32_t, hir::SpecialPipelineVariable> &&index) -> Result<void, LegaliseError> {
        const bool is_output = function->is_special_function(hir::SpecialFunction::VertexEntry);
        auto variable = m_root.allocate<hir::PipelineVariable>(vull::move(index), is_output);
        variable->set_type(type);
        VULL_TRY(scope.put_symbol(name, vull::move(variable)));
        return {};
    };

    // Create pipeline variables for explicit declarations.
    for (uint32_t i = 0; i < m_pipeline_decls.size(); i++) {
        const ast::PipelineDecl &decl = m_pipeline_decls[i];
        VULL_TRY(create_pipeline_variable(decl.name(), decl.type(), i));
    }

    if (function->is_special_function(hir::SpecialFunction::VertexEntry)) {
        VULL_TRY(create_pipeline_variable("gl_Position", Type(ScalarType::Float, 4),
                                          hir::SpecialPipelineVariable::Position));
    }

    // Lower body block.
    function->set_body(VULL_TRY(lower_block(ast_decl.block())));
    return function;
}

Result<void, LegaliseError> Legaliser::lower_pipeline_decl(const ast::PipelineDecl &ast_decl) {
    m_pipeline_decls.push(ast_decl);
    return {};
}

Result<void, LegaliseError> Legaliser::lower_top_level(const ast::Node &ast_node) {
    switch (ast_node.kind()) {
        using enum ast::NodeKind;
    case Aggregate: {
        const auto &aggregate = static_cast<const ast::Aggregate &>(ast_node);
        VULL_ASSERT(aggregate.kind() == ast::AggregateKind::UniformBlock);
        return {};
    }
    case FunctionDecl: {
        const auto &function_decl = static_cast<const ast::FunctionDecl &>(ast_node);
        m_root.append_top_level(VULL_TRY(lower_function_decl(function_decl)));
        return {};
    }
    case PipelineDecl: {
        VULL_TRY(lower_pipeline_decl(static_cast<const ast::PipelineDecl &>(ast_node)));
        return {};
    }
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

Result<void, LegaliseError> Legaliser::legalise(const ast::Root &ast_root) {
    for (const auto &ast_node : ast_root.top_level_nodes()) {
        VULL_TRY(lower_top_level(*ast_node));
    }
    return {};
}

} // namespace vull::shaderc
