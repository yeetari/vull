#include "Ast.hh"

#include <vull/support/Format.hh>

#include <stdio.h>

namespace ast {
namespace {

class DestroyVisitor final : public Visitor {
    Arena &m_arena;

public:
    explicit DestroyVisitor(Arena &arena) : m_arena(arena) {}

    void visit(const Aggregate &) override;
    void visit(const Constant &) override;
    void visit(const Function &) override;
    void visit(const ReturnStmt &) override;
};

void DestroyVisitor::visit(const Aggregate &aggregate) {
    for (const auto *node : aggregate.nodes()) {
        node->accept(*this);
    }
    m_arena.destroy(&aggregate);
}

void DestroyVisitor::visit(const Constant &constant) {
    m_arena.destroy(&constant);
}

void DestroyVisitor::visit(const Function &function) {
    function.block().accept(*this);
    m_arena.destroy(&function);
}

void DestroyVisitor::visit(const ReturnStmt &return_stmt) {
    return_stmt.expr().accept(*this);
    m_arena.destroy(&return_stmt);
}

} // namespace

Root::~Root() {
    DestroyVisitor destroy_visitor(m_arena);
    traverse(destroy_visitor);
}

void Root::append_top_level(Node *node) {
    m_top_level_nodes.push(node);
}

void Root::traverse(Visitor &visitor) {
    for (const auto *node : m_top_level_nodes) {
        node->accept(visitor);
    }
}

template <typename... Args>
void Formatter::print(const char *fmt, Args &&...args) {
    for (size_t i = 0; i < m_depth; i++) {
        fputs("    ", stderr);
    }
    auto string = vull::format(fmt, vull::forward<Args>(args)...);
    fwrite(string.data(), 1, string.length(), stderr);
}

void Formatter::visit(const Aggregate &aggregate) {
    VULL_ENSURE(aggregate.kind() == AggregateKind::Block);
    print(" {\n");
    m_depth++;
    for (const auto *node : aggregate.nodes()) {
        node->accept(*this);
    }
    m_depth--;
    print("}");
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
    print("fn {}()", function.name());
    function.block().accept(*this);
    print("\n");
}

void Formatter::visit(const ReturnStmt &return_stmt) {
    return_stmt.expr().accept(*this);
}

} // namespace ast
