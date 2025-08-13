#include <vull/shaderc/legaliser.hh>

#include <vull/container/hash_map.hh>
#include <vull/container/vector.hh>
#include <vull/shaderc/ast.hh>
#include <vull/shaderc/error.hh>
#include <vull/shaderc/hir.hh>
#include <vull/shaderc/source_location.hh>
#include <vull/shaderc/tree.hh>
#include <vull/shaderc/type.hh>
#include <vull/support/assert.hh>
#include <vull/support/optional.hh>
#include <vull/support/result.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/support/variant.hh>

#include <stdint.h>

// The legaliser turns the parsed AST into a high-level IR (HIR) which has a similar overall structure to the AST but
// with the following changes:
// 1) Symbols and lexical scoping in general is removed
// 2) All expressions become typed
// 3) Some higher level binary operations turn into more specific operations, e.g. multiplication could turn into
//    MatrixTimesVector

namespace vull::shaderc {
namespace {

template <typename T>
using LegaliseResult = Result<hir::NodeHandle<T>, Error>;

struct Symbol {
    hir::NodeHandle<hir::Expr> expr;
    SourceLocation source_location{0};
};

class Scope {
    Scope *&m_current;
    Scope *m_parent;
    HashMap<StringView, Symbol> m_symbol_map;

    Optional<const Symbol &> lookup(StringView name) const;

public:
    explicit Scope(Scope *&current);
    Scope(const Scope &) = delete;
    Scope(Scope &&) = delete;
    ~Scope();

    Scope &operator=(const Scope &) = delete;
    Scope &operator=(Scope &&) = delete;

    LegaliseResult<hir::Expr> lookup_symbol(StringView name, SourceLocation location) const;
    Result<void, Error> put_symbol(StringView name, hir::NodeHandle<hir::Expr> &&expr, SourceLocation location);
};

class Legaliser {
    hir::Root &m_root;
    Scope *m_scope{nullptr};
    UniquePtr<Scope> m_root_scope;
    Vector<const ast::IoDecl &> m_pipeline_decls;

    LegaliseResult<hir::Expr> lower_binary_expr(const ast::BinaryExpr &);
    LegaliseResult<hir::Expr> lower_constant(const ast::Constant &);
    LegaliseResult<hir::Expr> lower_construct_expr(const ast::Aggregate &);
    LegaliseResult<hir::Expr> lower_symbol(const ast::Symbol &);
    LegaliseResult<hir::Expr> lower_expr(const ast::Node &);

    LegaliseResult<hir::Node> lower_decl_stmt(const ast::DeclStmt &);
    LegaliseResult<hir::Node> lower_return_stmt(const ast::ReturnStmt &);
    LegaliseResult<hir::Node> lower_stmt(const ast::Node &);
    LegaliseResult<hir::Aggregate> lower_block(const ast::Aggregate &);

    LegaliseResult<hir::FunctionDecl> lower_function_decl(const ast::FunctionDecl &);
    Result<void, Error> lower_io_decl(const ast::IoDecl &);

public:
    explicit Legaliser(hir::Root &root);

    Result<void, Error> lower_top_level(const ast::Node &);
};

Scope::Scope(Scope *&current) : m_current(current), m_parent(current) {
    current = this;
}

Scope::~Scope() {
    m_current = m_parent;
}

Optional<const Symbol &> Scope::lookup(StringView name) const {
    if (auto symbol = m_symbol_map.get(name)) {
        return symbol;
    }
    if (m_parent != nullptr) {
        return m_parent->lookup(name);
    }
    return vull::nullopt;
}

LegaliseResult<hir::Expr> Scope::lookup_symbol(StringView name, SourceLocation location) const {
    if (auto symbol = lookup(name)) {
        return symbol->expr.share();
    }
    Error error;
    error.add_error(location, vull::format("use of undeclared identifier '{}'", name));
    return error;
}

Result<void, Error> Scope::put_symbol(StringView name, hir::NodeHandle<hir::Expr> &&expr, SourceLocation location) {
    if (auto previous = lookup(name)) {
        Error error;
        error.add_error(location, vull::format("attempted redefinition of '{}'", name));
        error.add_note(previous->source_location, "previous definition was here");
        return error;
    }
    m_symbol_map.set(name, Symbol{vull::move(expr), location});
    return {};
}

Legaliser::Legaliser(hir::Root &root) : m_root(root), m_root_scope(new Scope(m_scope)) {}

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
    return VULL_TRY(m_scope->lookup_symbol(ast_symbol.name(), ast_symbol.source_location()));
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
    auto variable = m_root.allocate<hir::Expr>(hir::NodeKind::LocalVariable);
    variable->set_type(ast_stmt.value().type());

    // Generate assign for initialiser.
    auto initialiser = VULL_TRY(lower_expr(ast_stmt.value()));
    auto assign = m_root.allocate<hir::BinaryExpr>();
    assign->set_lhs(variable.share());
    assign->set_rhs(vull::move(initialiser));
    assign->set_op(hir::BinaryOp::Assign);
    assign->set_is_assign(true);

    VULL_TRY(m_scope->put_symbol(ast_stmt.name(), vull::move(variable), ast_stmt.source_location()));
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
    auto function = m_root.allocate<hir::FunctionDecl>(ast_decl.return_type());

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
        VULL_TRY(scope.put_symbol(parameter.name(), vull::move(argument), parameter.source_location()));
    }

    auto create_pipeline_variable = [&](StringView name, Type type,
                                        Variant<uint32_t, hir::SpecialPipelineVariable> &&index,
                                        SourceLocation source_location) -> Result<void, Error> {
        const bool is_output = function->is_special_function(hir::SpecialFunction::VertexEntry);
        auto variable = m_root.allocate<hir::PipelineVariable>(vull::move(index), is_output);
        variable->set_type(type);
        VULL_TRY(scope.put_symbol(name, vull::move(variable), source_location));
        return {};
    };

    // Create pipeline variables for explicit declarations.
    for (uint32_t i = 0; i < m_pipeline_decls.size(); i++) {
        const ast::IoDecl &decl = m_pipeline_decls[i];
        const auto &symbol = static_cast<const ast::Symbol &>(decl.symbol_or_block());
        VULL_TRY(create_pipeline_variable(symbol.name(), symbol.type(), i, symbol.source_location()));
    }

    if (function->is_special_function(hir::SpecialFunction::VertexEntry)) {
        // TODO: Handle source location here somehow.
        VULL_TRY(create_pipeline_variable("gl_Position", Type(ScalarType::Float, 4),
                                          hir::SpecialPipelineVariable::Position, SourceLocation(0)));
    }

    // Lower body block.
    function->set_body(VULL_TRY(lower_block(ast_decl.block())));
    return function;
}

Result<void, Error> Legaliser::lower_io_decl(const ast::IoDecl &ast_decl) {
    if (ast_decl.io_kind() == ast::IoKind::Pipeline) {
        m_pipeline_decls.push(ast_decl);
        return {};
    }
    VULL_ENSURE_NOT_REACHED("Handle uniforms");
}

Result<void, Error> Legaliser::lower_top_level(const ast::Node &ast_node) {
    switch (ast_node.kind()) {
    case ast::NodeKind::FunctionDecl: {
        const auto &function_decl = static_cast<const ast::FunctionDecl &>(ast_node);
        m_root.append_top_level(VULL_TRY(lower_function_decl(function_decl)));
        return {};
    }
    case ast::NodeKind::IoDecl:
        VULL_TRY(lower_io_decl(static_cast<const ast::IoDecl &>(ast_node)));
        return {};
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

} // namespace

Result<hir::Root, Error> legalise(const ast::Root &ast_root) {
    hir::Root hir_root;
    Legaliser legaliser(hir_root);
    for (const auto &ast_node : ast_root.top_level_nodes()) {
        VULL_TRY(legaliser.lower_top_level(*ast_node));
    }
    return hir_root;
}

} // namespace vull::shaderc
