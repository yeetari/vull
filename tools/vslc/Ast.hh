#pragma once

#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>

#include "Arena.hh"

namespace ast {

struct Visitor;

enum class ScalarType {
    Float,
    Uint,
};

class Type {
    ScalarType m_scalar_type;
    uint8_t m_vector_size;

public:
    Type() = default;
    Type(ScalarType scalar_type, uint8_t vector_size) : m_scalar_type(scalar_type), m_vector_size(vector_size) {}

    ScalarType scalar_type() const { return m_scalar_type; }
    uint8_t vector_size() const { return m_vector_size; }
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
    AggregateKind m_kind;
    vull::Vector<Node *> m_nodes;

public:
    explicit Aggregate(AggregateKind kind) : m_kind(kind) {}

    void accept(Visitor &visitor) const override;
    void append_node(Node *node) { m_nodes.push(node); }

    AggregateKind kind() const { return m_kind; }
    const vull::Vector<Node *> &nodes() const { return m_nodes; }
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

class Function final : public Node {
    vull::StringView m_name;
    Aggregate *m_block;

public:
    Function(vull::StringView name, Aggregate *block) : m_name(name), m_block(block) {}

    void accept(Visitor &visitor) const override;

    vull::StringView name() const { return m_name; }
    Aggregate &block() const { return *m_block; }
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

struct Visitor {
    virtual void visit(const Aggregate &) = 0;
    virtual void visit(const Constant &) = 0;
    virtual void visit(const Function &) = 0;
    virtual void visit(const ReturnStmt &) = 0;
};

class Formatter final : public Visitor {
    size_t m_depth{0};

    template <typename... Args>
    void print(const char *fmt, Args &&...args);

public:
    void visit(const Aggregate &) override;
    void visit(const Constant &) override;
    void visit(const Function &) override;
    void visit(const ReturnStmt &) override;
};

inline void Aggregate::accept(Visitor &visitor) const {
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

} // namespace ast
