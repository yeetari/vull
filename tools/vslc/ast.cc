#include "ast.hh"

#include <vull/container/vector.hh>

namespace ast {
namespace {

class DestroyVisitor final : public Traverser<TraverseOrder::PostOrder> {
    Arena &m_arena;

public:
    explicit DestroyVisitor(Arena &arena) : m_arena(arena) {}

    void visit(Aggregate &) override;
    void visit(BinaryExpr &) override;
    void visit(CallExpr &) override;
    void visit(Constant &) override;
    void visit(DeclStmt &) override;
    void visit(Function &) override;
    void visit(PipelineDecl &) override;
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

void DestroyVisitor::visit(CallExpr &call_expr) {
    for (auto *argument : call_expr.arguments()) {
        argument->traverse(*this);
    }
    m_arena.destroy(&call_expr);
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

void DestroyVisitor::visit(PipelineDecl &pipeline_decl) {
    m_arena.destroy(&pipeline_decl);
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

} // namespace

Root::~Root() {
    DestroyVisitor destroy_visitor(m_arena);
    traverse(destroy_visitor);
}

void Root::append_top_level(Node *node) {
    m_top_level_nodes.push(node);
}

} // namespace ast
