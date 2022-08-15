#include "Ast.hh"

#include <vull/support/Assert.hh>
#include <vull/support/Format.hh>
#include <vull/support/String.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>

#include <stdio.h>

namespace ast {
namespace {

class DestroyVisitor final : public Traverser<TraverseOrder::PostOrder> {
    Arena &m_arena;

public:
    explicit DestroyVisitor(Arena &arena) : m_arena(arena) {}

    void visit(Aggregate &) override;
    void visit(BinaryExpr &) override;
    void visit(Constant &) override;
    void visit(DeclStmt &) override;
    void visit(Function &) override;
    void visit(ReturnStmt &) override;
    void visit(Symbol &) override;
    void visit(Root &) override {}
    void visit(UnaryExpr &) override;
};

void DestroyVisitor::visit(Aggregate &aggregate) {
    for (auto *node : aggregate.nodes()) {
        node->traverse(*this);
    }
    m_arena.destroy(&aggregate);
}

void DestroyVisitor::visit(BinaryExpr &binary_expr) {
    m_arena.destroy(&binary_expr);
}

void DestroyVisitor::visit(Constant &constant) {
    m_arena.destroy(&constant);
}

void DestroyVisitor::visit(DeclStmt &decl_stmt) {
    m_arena.destroy(&decl_stmt);
}

void DestroyVisitor::visit(Function &function) {
    function.block().traverse(*this);
    m_arena.destroy(&function);
}

void DestroyVisitor::visit(ReturnStmt &return_stmt) {
    m_arena.destroy(&return_stmt);
}

void DestroyVisitor::visit(Symbol &symbol) {
    m_arena.destroy(&symbol);
}

void DestroyVisitor::visit(UnaryExpr &unary_expr) {
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

void Formatter::visit(Aggregate &aggregate) {
    switch (aggregate.kind()) {
    case AggregateKind::Block:
        print_depth(m_depth++);
        print(" {\n");
        for (auto *node : aggregate.nodes()) {
            print_depth(m_depth);
            node->traverse(*this);
            print("\n");
        }
        print_depth(--m_depth);
        print("}");
        break;
    case AggregateKind::ConstructExpr:
        print("{}(", type_string(aggregate.type()));
        for (bool first = true; auto *node : aggregate.nodes()) {
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

void Formatter::visit(BinaryExpr &binary_expr) {
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
    case BinaryOp::Assign:
        print(" = ");
        break;
    }
    binary_expr.rhs().traverse(*this);
    print(")");
}

void Formatter::visit(Constant &constant) {
    switch (constant.scalar_type()) {
    case ScalarType::Float:
        print("{}f", constant.decimal());
        break;
    case ScalarType::Uint:
        print("{}u", constant.integer());
        break;
    }
}

void Formatter::visit(DeclStmt &decl_stmt) {
    print("let {} = ", decl_stmt.name());
    decl_stmt.value().traverse(*this);
    print(";");
}

void Formatter::visit(Function &function) {
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

void Formatter::visit(ReturnStmt &return_stmt) {
    return_stmt.expr().traverse(*this);
}

void Formatter::visit(Root &root) {
    for (auto *node : root.top_level_nodes()) {
        node->traverse(*this);
    }
}

void Formatter::visit(Symbol &symbol) {
    print("{}", symbol.name());
}

void Formatter::visit(UnaryExpr &unary_expr) {
    switch (unary_expr.op()) {
    case UnaryOp::Negate:
        print("-");
        break;
    }
    unary_expr.expr().traverse(*this);
}

} // namespace ast
