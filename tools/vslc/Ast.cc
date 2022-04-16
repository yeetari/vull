#include "Ast.hh"

#include <vull/support/Format.hh>

#include <stdio.h>

namespace ast {
namespace {

class DestroyVisitor final : public Visitor {
    Arena &m_arena;

public:
    explicit DestroyVisitor(Arena &arena) : m_arena(arena) {}

    void visit(const Block &block) override;
    void visit(const ConstantList &constant_list) override;
    void visit(const Function &function) override;
    void visit(const ReturnStmt &return_stmt) override;
};

void DestroyVisitor::visit(const Block &block) {
    block.traverse(*this);
    m_arena.destroy(&block);
}

void DestroyVisitor::visit(const ConstantList &constant_list) {
    m_arena.destroy(&constant_list);
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

void Block::append_node(Node *node) {
    m_nodes.push(node);
}

void Block::traverse(Visitor &visitor) const {
    for (const auto *node : m_nodes) {
        node->accept(visitor);
    }
}

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

void Formatter::visit(const Block &block) {
    print(" {\n");
    m_depth++;
    block.traverse(*this);
    m_depth--;
    print("}");
}

void Formatter::visit(const ConstantList &constant_list) {
    if (constant_list.type().vector_size() > 1) {
        print("vec{}(", static_cast<size_t>(constant_list.type().vector_size()));
    }
    for (bool first = true; const auto &constant : constant_list) {
        VULL_ENSURE(constant.scalar_type == ScalarType::Float);
        print("{}{}f", !first ? "," : "", constant.literal.decimal);
        first = false;
    }
    print("{}\n", constant_list.type().vector_size() > 1 ? ")" : "");
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
