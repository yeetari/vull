#include "Ast.hh"

#include <vull/support/Format.hh>

#include <stdio.h>

namespace ast {
namespace {

class DestroyVisitor final : public Traverser<TraverseOrder::PostOrder> {
    Arena &m_arena;

public:
    explicit DestroyVisitor(Arena &arena) : m_arena(arena) {}

    void visit(const Aggregate &) override;
    void visit(const BinaryExpr &) override;
    void visit(const Constant &) override;
    void visit(const Function &) override;
    void visit(const ReturnStmt &) override;
    void visit(const Symbol &) override;
    void visit(const Root &) override {}
    void visit(const UnaryExpr &) override;
};

void DestroyVisitor::visit(const Aggregate &aggregate) {
    for (const auto *node : aggregate.nodes()) {
        node->traverse(*this);
    }
    m_arena.destroy(&aggregate);
}

void DestroyVisitor::visit(const BinaryExpr &binary_expr) {
    m_arena.destroy(&binary_expr);
}

void DestroyVisitor::visit(const Constant &constant) {
    m_arena.destroy(&constant);
}

void DestroyVisitor::visit(const Function &function) {
    function.block().traverse(*this);
    m_arena.destroy(&function);
}

void DestroyVisitor::visit(const ReturnStmt &return_stmt) {
    m_arena.destroy(&return_stmt);
}

void DestroyVisitor::visit(const Symbol &symbol) {
    m_arena.destroy(&symbol);
}

void DestroyVisitor::visit(const UnaryExpr &unary_expr) {
    m_arena.destroy(&unary_expr);
}

template <typename... Args>
void print(const char *fmt, Args &&...args) {
    auto string = vull::format(fmt, vull::forward<Args>(args)...);
    fwrite(string.data(), 1, string.length(), stderr);
}

void print_depth(size_t depth) {
    for (size_t i = 0; i < depth; i++) {
        fputs("    ", stderr);
    }
}

vull::String type_string(const Type &type) {
    if (type.vector_size() == 1) {
        switch (type.scalar_type()) {
        case ScalarType::Float:
            return "float";
        case ScalarType::Uint:
            return "uint";
        }
    }
    switch (type.scalar_type()) {
    case ScalarType::Float:
        return vull::format("vec{}", static_cast<size_t>(type.vector_size()));
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

} // namespace

Root::~Root() {
    DestroyVisitor destroy_visitor(m_arena);
    traverse(destroy_visitor);
}

void Root::append_top_level(Node *node) {
    m_top_level_nodes.push(node);
}

void Formatter::visit(const Aggregate &aggregate) {
    switch (aggregate.kind()) {
    case AggregateKind::Block:
        print_depth(m_depth++);
        print(" {\n");
        for (const auto *node : aggregate.nodes()) {
            print_depth(m_depth);
            node->traverse(*this);
            print("\n");
        }
        print_depth(--m_depth);
        print("}");
        break;
    case AggregateKind::ConstructExpr:
        print("{}(", type_string(aggregate.type()));
        for (bool first = true; const auto *node : aggregate.nodes()) {
            if (!first) {
                print(", ");
            }
            node->traverse(*this);
            first = false;
        }
        print(")");
        break;
    }
}

void Formatter::visit(const BinaryExpr &binary_expr) {
    print("(");
    binary_expr.lhs().traverse(*this);
    switch (binary_expr.op()) {
    case BinaryOp::Add:
        print(" + ");
        break;
    case BinaryOp::Sub:
        print(" - ");
        break;
    case BinaryOp::Mul:
        print(" * ");
        break;
    case BinaryOp::Div:
        print(" / ");
        break;
    case BinaryOp::Mod:
        print(" % ");
        break;
    }
    binary_expr.rhs().traverse(*this);
    print(")");
}

void Formatter::visit(const Constant &constant) {
    switch (constant.scalar_type()) {
    case ScalarType::Float:
        print("{}f", constant.decimal());
        break;
    case ScalarType::Uint:
        print("{}u", constant.integer());
        break;
    }
}

void Formatter::visit(const Function &function) {
    print("fn {}(", function.name());
    for (bool first = true; const auto &parameter : function.parameters()) {
        if (!first) {
            print(", ");
        }
        print("let {}: {}", parameter.name(), type_string(parameter.type()));
        first = false;
    }
    print("): {}", type_string(function.return_type()));
    function.block().traverse(*this);
    print("\n");
}

void Formatter::visit(const ReturnStmt &return_stmt) {
    return_stmt.expr().traverse(*this);
}

void Formatter::visit(const Symbol &symbol) {
    print("{}", symbol.name());
}

void Formatter::visit(const UnaryExpr &unary_expr) {
    switch (unary_expr.op()) {
    case UnaryOp::Negate:
        print("-");
        break;
    }
    unary_expr.expr().traverse(*this);
}

} // namespace ast
