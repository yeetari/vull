#pragma once

#include <vull/container/vector.hh>
#include <vull/shaderc/arena.hh>
#include <vull/shaderc/type.hh>
#include <vull/support/assert.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>

#include <stddef.h>

namespace vull::shaderc::ast {

enum class TraverseOrder {
    None,
    PreOrder,
    PostOrder,
};

template <TraverseOrder Order>
struct Traverser;

struct Node {
    virtual void traverse(Traverser<TraverseOrder::None> &) = 0;
    virtual void traverse(Traverser<TraverseOrder::PreOrder> &) = 0;
    virtual void traverse(Traverser<TraverseOrder::PostOrder> &) = 0;
    virtual Type type() const { VULL_ENSURE_NOT_REACHED(); }
};

template <typename T>
class NodeHandle {
    Arena &m_arena;
    T *m_node;

public:
    NodeHandle(Arena &arena, T *node) : m_arena(arena), m_node(node) {}
    NodeHandle(const NodeHandle &) = delete;
    NodeHandle(NodeHandle &&other) : m_arena(other.m_arena), m_node(vull::exchange(other.m_node, nullptr)) {}
    ~NodeHandle();

    NodeHandle &operator=(const NodeHandle &) = delete;
    NodeHandle &operator=(NodeHandle &&) = delete;

    operator NodeHandle<Node>() const && { return NodeHandle<Node>(m_arena, m_node); }

    T *disown() { return vull::exchange(m_node, nullptr); }
    T *operator->() const { return m_node; }
};

template <typename T>
NodeHandle(T) -> NodeHandle<T>;

class TypedNode : public Node {
    Type m_type;

public:
    void set_type(const Type &type) { m_type = type; }
    Type type() const final { return m_type; }
};

enum class AggregateKind {
    Block,
    ConstructExpr,
    UniformBlock,
};

class Aggregate final : public TypedNode {
    Vector<Node *> m_nodes;
    AggregateKind m_kind;

public:
    explicit Aggregate(AggregateKind kind) : m_kind(kind) {}

    void append_node(NodeHandle<Node> &&handle) { m_nodes.push(handle.disown()); }
    void traverse(Traverser<TraverseOrder::None> &) override;
    void traverse(Traverser<TraverseOrder::PreOrder> &) override;
    void traverse(Traverser<TraverseOrder::PostOrder> &) override;

    AggregateKind kind() const { return m_kind; }
    const Vector<Node *> &nodes() const { return m_nodes; }
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

    // Parsed-generated Muls can be turned into these by the legaliser.
    VectorTimesScalar,
    MatrixTimesScalar,
    VectorTimesMatrix,
    MatrixTimesVector,
    MatrixTimesMatrix,
};

class BinaryExpr final : public TypedNode {
    Node *m_lhs;
    Node *m_rhs;
    BinaryOp m_op;

public:
    BinaryExpr(BinaryOp op, NodeHandle<Node> &&lhs, NodeHandle<Node> &&rhs)
        : m_lhs(lhs.disown()), m_rhs(rhs.disown()), m_op(op) {}

    void set_op(BinaryOp op) { m_op = op; }
    void traverse(Traverser<TraverseOrder::None> &) override;
    void traverse(Traverser<TraverseOrder::PreOrder> &) override;
    void traverse(Traverser<TraverseOrder::PostOrder> &) override;

    BinaryOp op() const { return m_op; }
    Node &lhs() const { return *m_lhs; }
    Node &rhs() const { return *m_rhs; }
};

class CallExpr final : public TypedNode {
    StringView m_name;
    Vector<Node *> m_arguments;

public:
    explicit CallExpr(StringView name) : m_name(name) {}

    void append_argument(NodeHandle<Node> &&argument) { m_arguments.push(argument.disown()); }
    void traverse(Traverser<TraverseOrder::None> &) override;
    void traverse(Traverser<TraverseOrder::PreOrder> &) override;
    void traverse(Traverser<TraverseOrder::PostOrder> &) override;

    StringView name() const { return m_name; }
    const Vector<Node *> &arguments() const { return m_arguments; }
};

class Constant final : public Node {
    union {
        float decimal;
        size_t integer;
    } m_literal;
    ScalarType m_scalar_type;

public:
    explicit Constant(float decimal) : m_literal({.decimal = decimal}), m_scalar_type(ScalarType::Float) {}
    explicit Constant(size_t integer) : m_literal({.integer = integer}), m_scalar_type(ScalarType::Uint) {}

    void traverse(Traverser<TraverseOrder::None> &) override;
    void traverse(Traverser<TraverseOrder::PreOrder> &) override;
    void traverse(Traverser<TraverseOrder::PostOrder> &) override;
    Type type() const override { return {m_scalar_type, 1, 1}; }

    float decimal() const { return m_literal.decimal; }
    size_t integer() const { return m_literal.integer; }
    ScalarType scalar_type() const { return m_scalar_type; }
};

class DeclStmt final : public Node {
    StringView m_name;
    Node *m_value;

public:
    DeclStmt(StringView name, NodeHandle<Node> &&value) : m_name(name), m_value(value.disown()) {}

    void traverse(Traverser<TraverseOrder::None> &) override;
    void traverse(Traverser<TraverseOrder::PreOrder> &) override;
    void traverse(Traverser<TraverseOrder::PostOrder> &) override;

    StringView name() const { return m_name; }
    Node &value() const { return *m_value; }
};

class Parameter {
    StringView m_name;
    Type m_type;

public:
    Parameter(StringView name, const Type &type) : m_name(name), m_type(type) {}

    StringView name() const { return m_name; }
    const Type &type() const { return m_type; }
};

class FunctionDecl final : public Node {
    StringView m_name;
    Aggregate *m_block;
    Type m_return_type;
    Vector<Parameter> m_parameters;

public:
    FunctionDecl(StringView name, NodeHandle<Aggregate> block, const Type &return_type, Vector<Parameter> &&parameters)
        : m_name(name), m_block(block.disown()), m_return_type(return_type), m_parameters(vull::move(parameters)) {}

    void traverse(Traverser<TraverseOrder::None> &) override;
    void traverse(Traverser<TraverseOrder::PreOrder> &) override;
    void traverse(Traverser<TraverseOrder::PostOrder> &) override;

    StringView name() const { return m_name; }
    Aggregate &block() const { return *m_block; }
    const Type &return_type() const { return m_return_type; }
    const Vector<Parameter> &parameters() const { return m_parameters; }
};

class PipelineDecl final : public TypedNode {
    StringView m_name;

public:
    PipelineDecl(StringView name, const Type &type) : m_name(name) { set_type(type); }

    void traverse(Traverser<TraverseOrder::None> &) override;
    void traverse(Traverser<TraverseOrder::PreOrder> &) override;
    void traverse(Traverser<TraverseOrder::PostOrder> &) override;

    StringView name() const { return m_name; }
};

class ReturnStmt final : public Node {
    Node *m_expr;

public:
    explicit ReturnStmt(NodeHandle<Node> &&expr) : m_expr(expr.disown()) {}

    void traverse(Traverser<TraverseOrder::None> &) override;
    void traverse(Traverser<TraverseOrder::PreOrder> &) override;
    void traverse(Traverser<TraverseOrder::PostOrder> &) override;

    Node &expr() const { return *m_expr; }
};

class Root final : public Node {
    Arena m_arena;
    Vector<Node *> m_top_level_nodes;

public:
    Root() = default;
    Root(const Root &) = delete;
    Root(Root &&) = default;
    ~Root();

    Root &operator=(const Root &) = delete;
    Root &operator=(Root &&) = delete;

    template <typename T, typename... Args>
    NodeHandle<T> allocate(Args &&...args) {
        return NodeHandle(m_arena, m_arena.allocate<T>(vull::forward<Args>(args)...));
    }

    void append_top_level(NodeHandle<Node> &&node);
    void traverse(Traverser<TraverseOrder::None> &) override;
    void traverse(Traverser<TraverseOrder::PreOrder> &) override;
    void traverse(Traverser<TraverseOrder::PostOrder> &) override;
    const Vector<Node *> &top_level_nodes() const { return m_top_level_nodes; }
};

class Symbol final : public TypedNode {
    StringView m_name;

public:
    explicit Symbol(StringView name) : m_name(name) {}

    void traverse(Traverser<TraverseOrder::None> &) override;
    void traverse(Traverser<TraverseOrder::PreOrder> &) override;
    void traverse(Traverser<TraverseOrder::PostOrder> &) override;

    StringView name() const { return m_name; }
};

enum class UnaryOp {
    Negate,
};

class UnaryExpr final : public TypedNode {
    Node *m_expr;
    UnaryOp m_op;

public:
    UnaryExpr(UnaryOp op, NodeHandle<Node> &&expr) : m_expr(expr.disown()), m_op(op) {}

    void traverse(Traverser<TraverseOrder::None> &) override;
    void traverse(Traverser<TraverseOrder::PreOrder> &) override;
    void traverse(Traverser<TraverseOrder::PostOrder> &) override;

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
    virtual void visit(PipelineDecl &) = 0;
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
    void visit(PipelineDecl &) override;
    void visit(ReturnStmt &) override;
    void visit(Symbol &) override;
    void visit(Root &) override;
    void visit(UnaryExpr &) override;
};

template <typename T>
NodeHandle<T>::~NodeHandle() {
    if (m_node != nullptr) {
        m_arena.destroy(m_node);
    }
}

// NOLINTBEGIN
template <>
inline NodeHandle<Node>::~NodeHandle() = default;
// NOLINTEND

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
