#pragma once

#include <vull/container/vector.hh>
#include <vull/shaderc/arena.hh>
#include <vull/shaderc/type.hh>
#include <vull/support/variant.hh>

#include <stdint.h>

namespace vull::shaderc::hir {

enum class NodeKind {
    FunctionDecl,

    ReturnStmt,

    BinaryExpr,
    BuiltinExpr,
    Constant,
    UnaryExpr,

    LocalVar,
    PipelineVar,
};

class Node {
    const NodeKind m_kind;

public:
    explicit Node(NodeKind kind) : m_kind(kind) {}

    NodeKind kind() const { return m_kind; }
};

class Aggregate : public Node {
    Vector<Node *> m_nodes;

public:
    explicit Aggregate(NodeKind kind) : Node(kind) {}

    void append_node(Node *node) { m_nodes.push(node); }
};

class Expr : public Node {
    Type m_type;

public:
    explicit Expr(NodeKind kind) : Node(kind) {}

    void set_type(Type type) { m_type = type; }
    Type type() const { return m_type; }
};

enum class SpecialFunction {
    None,
    VertexEntry,
    FragmentEntry,
};

class FunctionDecl : public Node {
    Type m_return_type;
    Vector<Type> m_parameter_types;
    SpecialFunction m_special_function;

public:
    SpecialFunction special_function() const { return m_special_function; }
};

class ReturnStmt : public Node {
    Expr *m_expr;

public:
    Expr *expr() const { return m_expr; }
};

enum class BinaryOp {
    Assign,
    Add,
    Sub,
    Div,
    Mod,
    ScalarTimesScalar,
    VectorTimesScalar,
    MatrixTimesScalar,
    VectorTimesMatrix,
    MatrixTimesVector,
    MatrixTimesMatrix,
};

class BinaryExpr : public Expr {
    Expr *m_lhs;
    Expr *m_rhs;
    BinaryOp m_op;
    bool m_is_assign{false};

public:
    BinaryExpr(Expr *lhs, Expr *rhs) : Expr(NodeKind::BinaryExpr), m_lhs(lhs), m_rhs(rhs) {}

    void set_op(BinaryOp op) { m_op = op; }

    BinaryOp op() const { return m_op; }
    Expr *lhs() const { return m_lhs; }
    Expr *rhs() const { return m_rhs; }
    bool is_assign() const { return m_is_assign; }
};

enum class BuiltinFunction {
    Dot,
    Max,
};

class BuiltinExpr : public Expr {
    BuiltinFunction m_function;

public:
    BuiltinExpr(BuiltinFunction function) : Expr(NodeKind::BuiltinExpr), m_function(function) {}

    BuiltinFunction function() const { return m_function; }
};

class Constant : public Expr {
    union {
        float decimal;
        size_t integer;
    } m_literal;

public:
    explicit Constant(float decimal) : Expr(NodeKind::Constant), m_literal({.decimal = decimal}) {}
    explicit Constant(size_t integer) : Expr(NodeKind::Constant), m_literal({.integer = integer}) {}

    float decimal() const { return m_literal.decimal; }
    size_t integer() const { return m_literal.integer; }
};

enum class UnaryOp {
    Negate,
};

class UnaryExpr : public Expr {
    Expr *m_expr;
    UnaryOp m_op;

public:
    UnaryOp op() const { return m_op; }
    Expr *expr() const { return m_expr; }
};

class Root {
    Arena m_arena;
    Vector<Node *> m_top_level_nodes;

public:
    template <typename T, typename... Args>
    T *allocate(Args &&...args) {
        return m_arena.allocate<T>(vull::forward<Args>(args)...);
    }

    void append_top_level(Node *node) {
        m_top_level_nodes.push(node);
    }

    const Vector<Node *> &top_level_nodes() const { return m_top_level_nodes; }
};

} // namespace vull::shaderc::hir
