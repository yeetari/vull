#include <vull/shaderc/ast.hh>

#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/shaderc/arena.hh>
#include <vull/shaderc/type.hh>
#include <vull/support/enum.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/string_view.hh>

namespace vull::shaderc::ast {
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
    void visit(FunctionDecl &) override;
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

void DestroyVisitor::visit(FunctionDecl &function_decl) {
    function_decl.block().traverse(*this);
    m_arena.destroy(&function_decl);
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

#define DEFINE_SIMPLE_TRAVERSE(node)                                                                                   \
    void node::traverse(Traverser<TraverseOrder::None> &traverser) {                                                   \
        traverser.visit(*this);                                                                                        \
    }                                                                                                                  \
    void node::traverse(Traverser<TraverseOrder::PreOrder> &traverser) {                                               \
        traverser.visit(*this);                                                                                        \
    }                                                                                                                  \
    void node::traverse(Traverser<TraverseOrder::PostOrder> &traverser) {                                              \
        traverser.visit(*this);                                                                                        \
    }

// Aggregates and functions usually require special handling.
DEFINE_SIMPLE_TRAVERSE(Aggregate)
DEFINE_SIMPLE_TRAVERSE(CallExpr)
DEFINE_SIMPLE_TRAVERSE(Constant)
DEFINE_SIMPLE_TRAVERSE(FunctionDecl)
DEFINE_SIMPLE_TRAVERSE(PipelineDecl)
DEFINE_SIMPLE_TRAVERSE(Symbol)
#undef DEFINE_SIMPLE_TRAVERSE

void BinaryExpr::traverse(Traverser<TraverseOrder::None> &traverser) {
    traverser.visit(*this);
}

void BinaryExpr::traverse(Traverser<TraverseOrder::PreOrder> &traverser) {
    traverser.visit(*this);
    m_lhs->traverse(traverser);
    m_rhs->traverse(traverser);
}

void BinaryExpr::traverse(Traverser<TraverseOrder::PostOrder> &traverser) {
    m_lhs->traverse(traverser);
    m_rhs->traverse(traverser);
    traverser.visit(*this);
}

void DeclStmt::traverse(Traverser<TraverseOrder::None> &traverser) {
    traverser.visit(*this);
}

void DeclStmt::traverse(Traverser<TraverseOrder::PreOrder> &traverser) {
    traverser.visit(*this);
    m_value->traverse(traverser);
}

void DeclStmt::traverse(Traverser<TraverseOrder::PostOrder> &traverser) {
    m_value->traverse(traverser);
    traverser.visit(*this);
}

void ReturnStmt::traverse(Traverser<TraverseOrder::None> &traverser) {
    traverser.visit(*this);
}

void ReturnStmt::traverse(Traverser<TraverseOrder::PreOrder> &traverser) {
    traverser.visit(*this);
    m_expr->traverse(traverser);
}

void ReturnStmt::traverse(Traverser<TraverseOrder::PostOrder> &traverser) {
    m_expr->traverse(traverser);
    traverser.visit(*this);
}

void Root::traverse(Traverser<TraverseOrder::None> &traverser) {
    traverser.visit(*this);
}

void Root::traverse(Traverser<TraverseOrder::PreOrder> &traverser) {
    traverser.visit(*this);
    for (auto *node : m_top_level_nodes) {
        node->traverse(traverser);
    }
}

void Root::traverse(Traverser<TraverseOrder::PostOrder> &traverser) {
    for (auto *node : m_top_level_nodes) {
        node->traverse(traverser);
    }
    traverser.visit(*this);
}

void UnaryExpr::traverse(Traverser<TraverseOrder::None> &traverser) {
    traverser.visit(*this);
}

void UnaryExpr::traverse(Traverser<TraverseOrder::PreOrder> &traverser) {
    traverser.visit(*this);
    m_expr->traverse(traverser);
}

void UnaryExpr::traverse(Traverser<TraverseOrder::PostOrder> &traverser) {
    m_expr->traverse(traverser);
    traverser.visit(*this);
}

void Dumper::print(StringView string) const {
    for (unsigned i = 0; i < m_indent; i++) {
        vull::print("  ");
    }
    vull::println(string);
}

void Dumper::visit(Aggregate &aggregate) {
    const auto type = aggregate.type();
    switch (aggregate.kind()) {
    case AggregateKind::Block:
        print("Block");
        break;
    case AggregateKind::ConstructExpr:
        print(vull::format("ConstructExpr({}, {}, {})", vull::enum_name(type.scalar_type()), type.matrix_rows(),
                           type.matrix_cols()));
        break;
    case AggregateKind::UniformBlock:
        print("UniformBlock");
        break;
    }

    m_indent++;
    for (auto *node : aggregate.nodes()) {
        node->traverse(*this);
    }
    m_indent--;
}

void Dumper::visit(BinaryExpr &binary_expr) {
    print(vull::format("BinaryExpr({})", vull::enum_name(binary_expr.op())));

    m_indent++;
    binary_expr.lhs().traverse(*this);
    binary_expr.rhs().traverse(*this);
    m_indent--;
}

void Dumper::visit(CallExpr &call_expr) {
    // TODO: Print type.
    print(vull::format("CallExpr({})", call_expr.name()));

    m_indent++;
    for (auto *argument : call_expr.arguments()) {
        argument->traverse(*this);
    }
    m_indent--;
}

void Dumper::visit(Constant &constant) {
    String string_value;
    switch (constant.scalar_type()) {
    case ScalarType::Float:
        string_value = vull::format("{}", constant.decimal());
        break;
    case ScalarType::Uint:
        string_value = vull::format("{}", constant.integer());
        break;
    }
    print(vull::format("Constant({}, {})", vull::enum_name(constant.scalar_type()), string_value));
}

void Dumper::visit(DeclStmt &decl_stmt) {
    print(vull::format("DeclStmt({})", decl_stmt.name()));
    m_indent++;
    decl_stmt.value().traverse(*this);
    m_indent--;
}

void Dumper::visit(FunctionDecl &function_decl) {
    // TODO: Return type.
    // TODO: Parameters.
    print(vull::format("FunctionDecl({})", function_decl.name()));
    m_indent++;
    function_decl.block().traverse(*this);
    m_indent--;
}

void Dumper::visit(PipelineDecl &pipeline_decl) {
    // TODO: Print type.
    print(vull::format("PipelineDecl({})", pipeline_decl.name()));
}

void Dumper::visit(ReturnStmt &return_stmt) {
    print("ReturnStmt");
    m_indent++;
    return_stmt.expr().traverse(*this);
    m_indent--;
}

void Dumper::visit(Symbol &symbol) {
    // TODO: Print type.
    print(vull::format("Symbol({})", symbol.name()));
}

void Dumper::visit(Root &root) {
    print("Root");
    m_indent++;
    for (auto *node : root.top_level_nodes()) {
        node->traverse(*this);
    }
}

void Dumper::visit(UnaryExpr &unary_expr) {
    print(vull::format("UnaryExpr({})", vull::enum_name(unary_expr.op())));
    m_indent++;
    unary_expr.expr().traverse(*this);
    m_indent--;
}

} // namespace vull::shaderc::ast
