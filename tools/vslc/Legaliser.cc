#include "Legaliser.hh"

#include <vull/container/Vector.hh>
#include <vull/support/Assert.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>

using namespace ast;

Legaliser::Scope::Scope(Scope *&current) : m_current(current), m_parent(current) {
    current = this;
}

Legaliser::Scope::~Scope() {
    m_current = m_parent;
}

Type Legaliser::Scope::lookup_symbol(vull::StringView name) const {
    for (const auto &symbol : m_symbol_map) {
        if (symbol.name == name) {
            return symbol.type;
        }
    }
    return ScalarType::Invalid;
}

void Legaliser::Scope::put_symbol(vull::StringView name, Type type) {
    m_symbol_map.push(Symbol{name, type});
}

void Legaliser::visit(Aggregate &aggregate) {
    for (auto *node : aggregate.nodes()) {
        node->traverse(*this);
    }
}

void Legaliser::visit(BinaryExpr &binary_expr) {
    const auto lhs = binary_expr.lhs().type();
    const auto rhs = binary_expr.rhs().type();

    if (binary_expr.op() == BinaryOp::Assign) {
        binary_expr.set_type(lhs);
        return;
    }

    const auto scalar_type = lhs.scalar_type();
    if ((lhs.is_vector() && rhs.is_scalar()) || (lhs.is_scalar() && rhs.is_vector())) {
        binary_expr.set_op(BinaryOp::VectorTimesScalar);
        binary_expr.set_type(lhs.is_vector() ? lhs : rhs);
    } else if ((lhs.is_matrix() && rhs.is_scalar()) || (lhs.is_scalar() && rhs.is_matrix())) {
        binary_expr.set_op(BinaryOp::MatrixTimesScalar);
        binary_expr.set_type(lhs.is_matrix() ? lhs : rhs);
    } else if (lhs.is_vector() && rhs.is_matrix()) {
        binary_expr.set_op(BinaryOp::VectorTimesMatrix);
        binary_expr.set_type(Type(scalar_type, rhs.matrix_cols()));
    } else if (lhs.is_matrix() && rhs.is_vector()) {
        binary_expr.set_op(BinaryOp::MatrixTimesVector);
        binary_expr.set_type(Type(scalar_type, lhs.matrix_rows()));
    } else if (lhs.is_matrix() && rhs.is_matrix()) {
        binary_expr.set_op(BinaryOp::MatrixTimesMatrix);
        binary_expr.set_type(Type(scalar_type, rhs.matrix_cols(), lhs.matrix_rows()));
    } else {
        VULL_ASSERT((lhs.is_scalar() && rhs.is_scalar()) || (lhs.is_vector() && rhs.is_vector()));
        binary_expr.set_type(scalar_type);
    }
}

void Legaliser::visit(DeclStmt &decl_stmt) {
    m_scope->put_symbol(decl_stmt.name(), decl_stmt.value().type());
}

void Legaliser::visit(Function &function) {
    Scope scope(m_scope);
    for (const auto &parameter : function.parameters()) {
        m_scope->put_symbol(parameter.name(), parameter.type());
    }
    function.block().traverse(*this);
}

void Legaliser::visit(ast::Symbol &symbol) {
    symbol.set_type(m_scope->lookup_symbol(symbol.name()));
}

void Legaliser::visit(ast::UnaryExpr &unary_expr) {
    unary_expr.set_type(unary_expr.expr().type());
}
