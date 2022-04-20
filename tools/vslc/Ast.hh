#pragma once

#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>

#include "Arena.hh"

namespace ast {

struct Visitor;

enum class ScalarType : uint8_t {
    Float,
    Uint,
};

class Type {
    ScalarType m_scalar_type;
    uint8_t m_vector_size : 4;
    uint8_t m_matrix_cols : 4;

public:
    Type() = default;
    Type(ScalarType scalar_type, uint8_t vector_size, uint8_t matrix_cols)
        : m_scalar_type(scalar_type), m_vector_size(vector_size), m_matrix_cols(matrix_cols) {}

    bool is_matrix() const { return m_matrix_cols > 1; }
    bool is_vector() const { return m_vector_size > 1 && !is_matrix(); }
    bool is_scalar() const { return !is_vector(); }

    ScalarType scalar_type() const { return m_scalar_type; }
    uint8_t vector_size() const { return m_vector_size; }
    uint8_t matrix_cols() const { return m_matrix_cols; }
};

struct Node {
    virtual void accept(Visitor &visitor) const = 0;
};

class TypedNode : public Node {
    Type m_type;

public:
    void set_type(const Type &type) { m_type = type; }
    const Type &type() const { return m_type; }
};

enum class AggregateKind {
    Block,
    ConstructExpr,
};

class Aggregate final : public TypedNode {
    vull::Vector<Node *> m_nodes;
    AggregateKind m_kind;

public:
    explicit Aggregate(AggregateKind kind) : m_kind(kind) {}

    void accept(Visitor &visitor) const override;
    void append_node(Node *node) { m_nodes.push(node); }

    AggregateKind kind() const { return m_kind; }
    const vull::Vector<Node *> &nodes() const { return m_nodes; }
};

enum class BinaryOp {
    Add,
    Sub,
    Mul,
    Div,
    Mod,
};

class BinaryExpr final : public Node {
    Node *m_lhs;
    Node *m_rhs;
    BinaryOp m_op;

public:
    BinaryExpr(BinaryOp op, Node *lhs, Node *rhs) : m_lhs(lhs), m_rhs(rhs), m_op(op) {}

    void accept(Visitor &visitor) const override;

    BinaryOp op() const { return m_op; }
    Node &lhs() const { return *m_lhs; }
    Node &rhs() const { return *m_rhs; }
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

    void accept(Visitor &visitor) const override;

    float decimal() const { return m_literal.decimal; }
    size_t integer() const { return m_literal.integer; }
    ScalarType scalar_type() const { return m_scalar_type; }
};

class Parameter {
    vull::StringView m_name;
    Type m_type;

public:
    Parameter(vull::StringView name, const Type &type) : m_name(name), m_type(type) {}

    vull::StringView name() const { return m_name; }
    const Type &type() const { return m_type; }
};

class Function final : public Node {
    vull::StringView m_name;
    Aggregate *m_block;
    Type m_return_type;
    vull::Vector<Parameter> m_parameters;

public:
    Function(vull::StringView name, Aggregate *block, const Type &return_type, vull::Vector<Parameter> &&parameters)
        : m_name(name), m_block(block), m_return_type(return_type), m_parameters(vull::move(parameters)) {}

    void accept(Visitor &visitor) const override;

    vull::StringView name() const { return m_name; }
    Aggregate &block() const { return *m_block; }
    const Type &return_type() const { return m_return_type; }
    const vull::Vector<Parameter> &parameters() const { return m_parameters; }
};

class ReturnStmt final : public Node {
    Node *m_expr;

public:
    explicit ReturnStmt(Node *expr) : m_expr(expr) {}

    void accept(Visitor &visitor) const override;

    Node &expr() const { return *m_expr; }
};

class Root {
    Arena m_arena;
    vull::Vector<Node *> m_top_level_nodes;

public:
    Root() = default;
    Root(const Root &) = delete;
    Root(Root &&) = default;
    ~Root();

    Root &operator=(const Root &) = delete;
    Root &operator=(Root &&) = delete;

    template <typename U, typename... Args>
    U *allocate(Args &&...args) {
        return m_arena.allocate<U>(vull::forward<Args>(args)...);
    }

    void append_top_level(Node *node);
    void traverse(Visitor &visitor);
};

class Symbol final : public Node {
    vull::StringView m_name;

public:
    explicit Symbol(vull::StringView name) : m_name(name) {}

    void accept(Visitor &visitor) const override;

    vull::StringView name() const { return m_name; }
};

enum class UnaryOp {
    Negate,
};

class UnaryExpr final : public Node {
    Node *m_expr;
    UnaryOp m_op;

public:
    UnaryExpr(UnaryOp op, Node *expr) : m_expr(expr), m_op(op) {}

    void accept(Visitor &visitor) const override;

    UnaryOp op() const { return m_op; }
    Node &expr() const { return *m_expr; }
};

struct Visitor {
    virtual void visit(const Aggregate &) = 0;
    virtual void visit(const BinaryExpr &) = 0;
    virtual void visit(const Constant &) = 0;
    virtual void visit(const Function &) = 0;
    virtual void visit(const ReturnStmt &) = 0;
    virtual void visit(const Symbol &) = 0;
    virtual void visit(const UnaryExpr &) = 0;
};

class Formatter final : public Visitor {
    size_t m_depth{0};

public:
    void visit(const Aggregate &) override;
    void visit(const BinaryExpr &) override;
    void visit(const Constant &) override;
    void visit(const Function &) override;
    void visit(const ReturnStmt &) override;
    void visit(const Symbol &) override;
    void visit(const UnaryExpr &) override;
};

inline void Aggregate::accept(Visitor &visitor) const {
    visitor.visit(*this);
}

inline void BinaryExpr::accept(Visitor &visitor) const {
    visitor.visit(*this);
}

inline void Constant::accept(Visitor &visitor) const {
    visitor.visit(*this);
}

inline void Function::accept(Visitor &visitor) const {
    visitor.visit(*this);
}

inline void ReturnStmt::accept(Visitor &visitor) const {
    visitor.visit(*this);
}

inline void Symbol::accept(Visitor &visitor) const {
    visitor.visit(*this);
}

inline void UnaryExpr::accept(Visitor &visitor) const {
    visitor.visit(*this);
}

} // namespace ast
