#pragma once

#include <vull/container/vector.hh>
#include <vull/shaderc/source_location.hh>
#include <vull/shaderc/tree.hh>
#include <vull/shaderc/type.hh>
#include <vull/support/assert.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>

#include <stddef.h>

namespace vull::shaderc::ast {

// TODO: Remove most typing from AST.

enum class TraverseOrder {
    None,
    PreOrder,
    PostOrder,
};

template <TraverseOrder Order>
struct Traverser;

enum class NodeKind {
    Root,
    FunctionDecl,
    IoDecl,

    DeclStmt,
    ReturnStmt,

    Aggregate,
    BinaryExpr,
    CallExpr,
    Constant,
    Symbol,
    UnaryExpr,
};

class Node : public tree::NodeBase {
    const NodeKind m_kind;
    const SourceLocation m_source_location;

protected:
    Node(NodeKind kind, SourceLocation source_location) : m_kind(kind), m_source_location(source_location) {}

public:
    NodeKind kind() const { return m_kind; }
    SourceLocation source_location() const { return m_source_location; }

    void destroy();
    void traverse(Traverser<TraverseOrder::None> &);
    void traverse(Traverser<TraverseOrder::PreOrder> &);
    void traverse(Traverser<TraverseOrder::PostOrder> &);
    virtual Type type() const { VULL_ENSURE_NOT_REACHED(); }
};

template <typename T>
using NodeHandle = tree::NodeHandle<Node, T>;

class TypedNode : public Node {
    Type m_type;

protected:
    using Node::Node;

public:
    void set_type(const Type &type) { m_type = type; }
    Type type() const final { return m_type; }
};

enum class AggregateKind {
    Block,
    ConstructExpr,
};

class Aggregate final : public TypedNode {
    Vector<NodeHandle<Node>> m_nodes;
    AggregateKind m_kind;

public:
    explicit Aggregate(SourceLocation source_location, AggregateKind kind)
        : TypedNode(NodeKind::Aggregate, source_location), m_kind(kind) {}

    void append_node(NodeHandle<Node> &&handle) { m_nodes.push(vull::move(handle)); }

    AggregateKind kind() const { return m_kind; }
    const Vector<NodeHandle<Node>> &nodes() const { return m_nodes; }
};

enum class BinaryOp {
    Add,
    Sub,
    Mul,
    Div,
    Mod,

    Assign,
    AddAssign,
    SubAssign,
    MulAssign,
    DivAssign,
};

class BinaryExpr final : public TypedNode {
    NodeHandle<Node> m_lhs;
    NodeHandle<Node> m_rhs;
    BinaryOp m_op;

public:
    BinaryExpr(BinaryOp op, NodeHandle<Node> &&lhs, NodeHandle<Node> &&rhs)
        : TypedNode(NodeKind::BinaryExpr, lhs->source_location()), m_lhs(vull::move(lhs)), m_rhs(vull::move(rhs)),
          m_op(op) {}

    void set_op(BinaryOp op) { m_op = op; }

    BinaryOp op() const { return m_op; }
    Node &lhs() const { return *m_lhs; }
    Node &rhs() const { return *m_rhs; }
};

class CallExpr final : public TypedNode {
    StringView m_name;
    Vector<NodeHandle<Node>> m_arguments;

public:
    CallExpr(SourceLocation source_location, StringView name)
        : TypedNode(NodeKind::CallExpr, source_location), m_name(name) {}

    void append_argument(NodeHandle<Node> &&argument) { m_arguments.push(vull::move(argument)); }

    StringView name() const { return m_name; }
    const Vector<NodeHandle<Node>> &arguments() const { return m_arguments; }
};

class Constant final : public Node {
    union {
        float decimal;
        size_t integer;
    } m_literal;
    ScalarType m_scalar_type;

public:
    Constant(SourceLocation source_location, float decimal)
        : Node(NodeKind::Constant, source_location), m_literal({.decimal = decimal}), m_scalar_type(ScalarType::Float) {
    }
    Constant(SourceLocation source_location, size_t integer)
        : Node(NodeKind::Constant, source_location), m_literal({.integer = integer}), m_scalar_type(ScalarType::Uint) {}

    Type type() const override { return {m_scalar_type, 1, 1}; }

    float decimal() const { return m_literal.decimal; }
    size_t integer() const { return m_literal.integer; }
    ScalarType scalar_type() const { return m_scalar_type; }
};

class DeclStmt final : public Node {
    StringView m_name;
    NodeHandle<Node> m_value;

public:
    DeclStmt(SourceLocation source_location, StringView name, NodeHandle<Node> &&value)
        : Node(NodeKind::DeclStmt, source_location), m_name(name), m_value(vull::move(value)) {}

    StringView name() const { return m_name; }
    Node &value() const { return *m_value; }
};

class Parameter {
    StringView m_name;
    Type m_type;
    SourceLocation m_source_location;

public:
    Parameter(SourceLocation source_location, StringView name, const Type &type)
        : m_name(name), m_type(type), m_source_location(source_location) {}

    StringView name() const { return m_name; }
    const Type &type() const { return m_type; }
    SourceLocation source_location() const { return m_source_location; }
};

class FunctionDecl final : public Node {
    StringView m_name;
    NodeHandle<Aggregate> m_block;
    Type m_return_type;
    Vector<Parameter> m_parameters;

public:
    FunctionDecl(SourceLocation source_location, StringView name, NodeHandle<Aggregate> block, const Type &return_type,
                 Vector<Parameter> &&parameters)
        : Node(NodeKind::FunctionDecl, source_location), m_name(name), m_block(vull::move(block)),
          m_return_type(return_type), m_parameters(vull::move(parameters)) {}

    StringView name() const { return m_name; }
    Aggregate &block() const { return *m_block; }
    const Type &return_type() const { return m_return_type; }
    const Vector<Parameter> &parameters() const { return m_parameters; }
};

enum class IoKind {
    Pipeline,
    Uniform,
};

class IoDecl final : public Node {
    NodeHandle<Node> m_symbol_or_block;
    IoKind m_io_kind;

public:
    IoDecl(SourceLocation source_location, IoKind io_kind, NodeHandle<Node> &&symbol_or_block)
        : Node(NodeKind::IoDecl, source_location), m_symbol_or_block(vull::move(symbol_or_block)), m_io_kind(io_kind) {}

    Node &symbol_or_block() const { return *m_symbol_or_block; }
    IoKind io_kind() const { return m_io_kind; }
};

class ReturnStmt final : public Node {
    NodeHandle<Node> m_expr;

public:
    ReturnStmt(SourceLocation source_location, NodeHandle<Node> &&expr)
        : Node(NodeKind::ReturnStmt, source_location), m_expr(vull::move(expr)) {}

    Node &expr() const { return *m_expr; }
};

class Root final : public Node {
    tree::Arena m_arena;
    Vector<NodeHandle<Node>> m_top_level_nodes;

public:
    Root() : Node(NodeKind::Root, SourceLocation(0)) {}

    template <typename T, typename... Args>
    NodeHandle<T> allocate(Args &&...args) {
        return NodeHandle<T>::create_new(m_arena.allocate<T>(vull::forward<Args>(args)...));
    }

    void append_top_level(NodeHandle<Node> &&node);
    const Vector<NodeHandle<Node>> &top_level_nodes() const { return m_top_level_nodes; }
};

class Symbol final : public TypedNode {
    StringView m_name;

public:
    Symbol(SourceLocation source_location, StringView name)
        : TypedNode(NodeKind::Symbol, source_location), m_name(name) {}
    Symbol(SourceLocation source_location, StringView name, Type type) : Symbol(source_location, name) {
        set_type(type);
    }

    StringView name() const { return m_name; }
};

enum class UnaryOp {
    Negate,
};

class UnaryExpr final : public TypedNode {
    NodeHandle<Node> m_expr;
    UnaryOp m_op;

public:
    UnaryExpr(UnaryOp op, NodeHandle<Node> &&expr)
        : TypedNode(NodeKind::UnaryExpr, expr->source_location()), m_expr(vull::move(expr)), m_op(op) {}

    UnaryOp op() const { return m_op; }
    Node &expr() const { return *m_expr; }
};

template <TraverseOrder Order>
struct Traverser {
    virtual void visit(Aggregate &) = 0;
    virtual void visit(BinaryExpr &) = 0;
    virtual void visit(CallExpr &) = 0;
    virtual void visit(Constant &) = 0;
    virtual void visit(DeclStmt &) = 0;
    virtual void visit(FunctionDecl &) = 0;
    virtual void visit(IoDecl &) = 0;
    virtual void visit(ReturnStmt &) = 0;
    virtual void visit(Root &) = 0;
    virtual void visit(Symbol &) = 0;
    virtual void visit(UnaryExpr &) = 0;
};

class Dumper final : public Traverser<TraverseOrder::None> {
    unsigned m_indent{0};

    void print(StringView string) const;

public:
    void visit(Aggregate &) override;
    void visit(BinaryExpr &) override;
    void visit(CallExpr &) override;
    void visit(Constant &) override;
    void visit(DeclStmt &) override;
    void visit(FunctionDecl &) override;
    void visit(IoDecl &) override;
    void visit(ReturnStmt &) override;
    void visit(Symbol &) override;
    void visit(Root &) override;
    void visit(UnaryExpr &) override;
};

constexpr bool is_assign_op(BinaryOp op) {
    switch (op) {
    case BinaryOp::Assign:
    case BinaryOp::AddAssign:
    case BinaryOp::SubAssign:
    case BinaryOp::MulAssign:
    case BinaryOp::DivAssign:
        return true;
    default:
        return false;
    }
}

} // namespace vull::shaderc::ast
