#pragma once

#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>

#include "Arena.hh"

namespace ast {

struct Visitor;

struct Node {
    virtual void accept(Visitor &visitor) const = 0;
};

struct CompoundNode : Node {
    virtual void traverse(Visitor &visitor) const = 0;
};

enum class ScalarType {
    Float,
};

class Type {
    ScalarType m_scalar_type;
    uint8_t m_vector_size;

public:
    Type(ScalarType scalar_type, uint8_t vector_size) : m_scalar_type(scalar_type), m_vector_size(vector_size) {}

    ScalarType scalar_type() const { return m_scalar_type; }
    uint8_t vector_size() const { return m_vector_size; }
};

class TypedNode : public Node {
    Type m_type;

public:
    explicit TypedNode(const Type &type) : m_type(type) {}

    void set_type(const Type &type) { m_type = type; }

    const Type &type() const { return m_type; }
};

class Block final : public CompoundNode {
    vull::Vector<Node *> m_nodes;

public:
    void append_node(Node *node);
    void accept(Visitor &visitor) const override;
    void traverse(Visitor &visitor) const override;
};

struct Constant {
    union {
        float decimal;
        size_t integer;
    } literal;
    ScalarType scalar_type;
};

// TODO(small-vector): Use a small vector - a constant list will more often than not have <= 4 elements.
struct ConstantList final : TypedNode, vull::Vector<Constant> {
    using TypedNode::TypedNode;
    void accept(Visitor &visitor) const override;
};

class Function final : public Node {
    vull::StringView m_name;
    Block *m_block;

public:
    Function(vull::StringView name, Block *block) : m_name(name), m_block(block) {}

    void accept(Visitor &visitor) const override;

    vull::StringView name() const { return m_name; }
    Block &block() const { return *m_block; }
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
    virtual void visit(const Block &) = 0;
    virtual void visit(const ConstantList &) = 0;
    virtual void visit(const Function &) = 0;
    virtual void visit(const ReturnStmt &) = 0;
};

class Formatter final : public Visitor {
    size_t m_depth{0};

    template <typename... Args>
    void print(const char *fmt, Args &&...args);

public:
    void visit(const Block &) override;
    void visit(const ConstantList &) override;
    void visit(const Function &) override;
    void visit(const ReturnStmt &) override;
};

inline void Block::accept(Visitor &visitor) const {
    visitor.visit(*this);
}

inline void ConstantList::accept(Visitor &visitor) const {
    visitor.visit(*this);
}

inline void Function::accept(Visitor &visitor) const {
    visitor.visit(*this);
}

inline void ReturnStmt::accept(Visitor &visitor) const {
    visitor.visit(*this);
}

} // namespace ast
