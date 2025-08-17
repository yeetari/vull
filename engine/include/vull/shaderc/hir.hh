#pragma once

#include <vull/container/vector.hh>
#include <vull/shaderc/tree.hh>
#include <vull/shaderc/type.hh>
#include <vull/support/optional.hh>
#include <vull/support/utility.hh>
#include <vull/support/variant.hh>

#include <stddef.h>
#include <stdint.h>

namespace vull {

class StringBuilder;

} // namespace vull

namespace vull::shaderc::hir {

enum class NodeKind {
    FunctionDecl,
    Block,

    ExprStmt,
    ReturnStmt,

    BinaryExpr,
    CallExpr,
    Constant,
    ConstructExpr,
    UnaryExpr,

    Argument,
    LocalVariable,
    PipelineVariable,
    PushConstant,
};

class Node : public tree::NodeBase {
    const NodeKind m_kind;

protected:
    explicit Node(NodeKind kind) : m_kind(kind) {}

public:
    void destroy();

    NodeKind kind() const { return m_kind; }
};

template <typename T>
using NodeHandle = tree::NodeHandle<Node, T>;

class Aggregate : public Node {
    Vector<NodeHandle<Node>> m_nodes;

public:
    explicit Aggregate(NodeKind kind) : Node(kind) {}

    void append_node(NodeHandle<Node> &&node) { m_nodes.push(vull::move(node)); }

    const Vector<NodeHandle<Node>> &nodes() const { return m_nodes; }
};

class Expr : public Node {
    Type m_type;

public:
    explicit Expr(NodeKind kind) : Node(kind) {}

    void set_type(Type type) { m_type = type; }
    Type type() const { return m_type; }
};

enum class ExtInstSet {
    GlslStd450,
};

class ExtInst {
    ExtInstSet m_inst_set;
    uint32_t m_opcode;

public:
    ExtInst(ExtInstSet inst_set, uint32_t opcode) : m_inst_set(inst_set), m_opcode(opcode) {}

    ExtInstSet inst_set() const { return m_inst_set; }
    uint32_t opcode() const { return m_opcode; }
};

enum class SpecialFunction {
    VertexEntry,
    FragmentEntry,
};

class FunctionDecl : public Node {
    Type m_return_type;
    Vector<Type> m_parameter_types;
    NodeHandle<Aggregate> m_body;
    Optional<ExtInst> m_ext_inst;
    Optional<SpecialFunction> m_special_function;

public:
    FunctionDecl(Type return_type, Vector<Type> &&parameter_types)
        : Node(NodeKind::FunctionDecl), m_return_type(return_type), m_parameter_types(vull::move(parameter_types)) {}

    void set_body(NodeHandle<Aggregate> &&body) { m_body = vull::move(body); }
    bool has_body() const { return !m_body.is_null(); }

    void set_ext_inst(ExtInst ext_inst) { m_ext_inst = ext_inst; }

    void set_special_function(SpecialFunction special_function) { m_special_function = special_function; }
    bool is_special_function(SpecialFunction special_function) const {
        return m_special_function && *m_special_function == special_function;
    }

    Type return_type() const { return m_return_type; }
    const Vector<Type> &parameter_types() const { return m_parameter_types; }
    Aggregate &body() const { return *m_body; }
    Optional<ExtInst> ext_inst() const { return m_ext_inst; }
    Optional<SpecialFunction> special_function() const { return m_special_function; }
};

class ExprStmt : public Node {
    NodeHandle<Expr> m_expr;

public:
    ExprStmt(NodeHandle<Expr> &&expr) : Node(NodeKind::ExprStmt), m_expr(vull::move(expr)) {}

    Expr &expr() const { return *m_expr; }
};

class ReturnStmt : public Node {
    NodeHandle<Expr> m_expr;

public:
    explicit ReturnStmt(NodeHandle<Expr> &&expr) : Node(NodeKind::ReturnStmt), m_expr(vull::move(expr)) {}

    Expr &expr() const { return *m_expr; }
};

enum class BinaryOp {
    Invalid,
    Assign,
    Add,
    Sub,
    Div,
    Mod,
    ScalarTimesScalar,
    VectorTimesScalar,
    VectorTimesVector,
    MatrixTimesScalar,
    VectorTimesMatrix,
    MatrixTimesVector,
    MatrixTimesMatrix,
};

class BinaryExpr : public Expr {
    NodeHandle<Expr> m_lhs;
    NodeHandle<Expr> m_rhs;
    BinaryOp m_op{BinaryOp::Invalid};
    bool m_is_assign{false};

public:
    BinaryExpr() : Expr(NodeKind::BinaryExpr) {}

    void set_lhs(NodeHandle<Expr> &&lhs) { m_lhs = vull::move(lhs); }
    void set_rhs(NodeHandle<Expr> &&rhs) { m_rhs = vull::move(rhs); }
    void set_op(BinaryOp op) { m_op = op; }
    void set_is_assign(bool is_assign) { m_is_assign = is_assign; }

    BinaryOp op() const { return m_op; }
    Expr &lhs() const { return *m_lhs; }
    Expr &rhs() const { return *m_rhs; }
    bool is_assign() const { return m_is_assign; }
};

class CallExpr : public Expr {
    NodeHandle<FunctionDecl> m_callee;
    Vector<NodeHandle<Expr>> m_arguments;

public:
    CallExpr(NodeHandle<FunctionDecl> &&callee, Vector<NodeHandle<Expr>> &&arguments)
        : Expr(NodeKind::CallExpr), m_callee(vull::move(callee)), m_arguments(vull::move(arguments)) {}

    FunctionDecl &callee() const { return *m_callee; }
    const Vector<NodeHandle<Expr>> &arguments() const { return m_arguments; }
};

class Constant : public Expr {
    size_t m_value;

public:
    Constant(size_t value, ScalarType scalar_type) : Expr(NodeKind::Constant), m_value(value) { set_type(scalar_type); }

    size_t value() const { return m_value; }
};

class ConstructExpr : public Expr {
    Vector<NodeHandle<Expr>> m_values;

public:
    ConstructExpr() : Expr(NodeKind::ConstructExpr) {}

    void append_value(NodeHandle<Expr> &&value) { m_values.push(vull::move(value)); }

    const Vector<NodeHandle<Expr>> &values() const { return m_values; }
};

enum class UnaryOp {
    Negate,
};

class UnaryExpr : public Expr {
    NodeHandle<Expr> m_expr;
    UnaryOp m_op;

public:
    UnaryExpr() : Expr(NodeKind::UnaryExpr) {}

    void set_op(UnaryOp op) { m_op = op; }
    void set_expr(NodeHandle<Expr> &&expr) { m_expr = vull::move(expr); }

    UnaryOp op() const { return m_op; }
    Expr &expr() const { return *m_expr; }
};

enum class SpecialPipelineVariable {
    Position,
};

class PipelineVariable : public Expr {
    Variant<uint32_t, SpecialPipelineVariable> m_index;
    bool m_is_output;

public:
    PipelineVariable(Variant<uint32_t, SpecialPipelineVariable> &&index, bool is_output)
        : Expr(NodeKind::PipelineVariable), m_index(vull::move(index)), m_is_output(is_output) {}

    const Variant<uint32_t, SpecialPipelineVariable> &index() const { return m_index; }
    bool is_output() const { return m_is_output; }
};

class Root {
    tree::Arena m_arena;
    Vector<NodeHandle<Node>> m_top_level_nodes;

public:
    template <typename T, typename... Args>
    NodeHandle<T> allocate(Args &&...args) {
        return NodeHandle<T>::create_new(m_arena.allocate<T>(vull::forward<Args>(args)...));
    }

    void append_top_level(NodeHandle<Node> &&node);
    const Vector<NodeHandle<Node>> &top_level_nodes() const { return m_top_level_nodes; }
};

void dump(const Root &root, StringBuilder &builder);

} // namespace vull::shaderc::hir
