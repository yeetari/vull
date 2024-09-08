#include <vull/shaderc/ast.hh>

#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/shaderc/tree.hh>
#include <vull/shaderc/type.hh>
#include <vull/support/enum.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>

namespace vull::shaderc::ast {
namespace {

class DestroyVisitor final : public Traverser<TraverseOrder::PostOrder> {
    tree::Arena &m_arena;

public:
    explicit DestroyVisitor(tree::Arena &arena) : m_arena(arena) {}

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

void Node::traverse(Traverser<TraverseOrder::None> &traverser) {
    switch (m_kind) {
        using enum NodeKind;
    case Root:
        traverser.visit(static_cast<ast::Root &>(*this));
        break;
    case FunctionDecl:
        traverser.visit(static_cast<ast::FunctionDecl &>(*this));
        break;
    case PipelineDecl:
        traverser.visit(static_cast<ast::PipelineDecl &>(*this));
        break;
    case DeclStmt:
        traverser.visit(static_cast<ast::DeclStmt &>(*this));
        break;
    case ReturnStmt:
        traverser.visit(static_cast<ast::ReturnStmt &>(*this));
        break;
    case Aggregate:
        traverser.visit(static_cast<ast::Aggregate &>(*this));
        break;
    case BinaryExpr:
        traverser.visit(static_cast<ast::BinaryExpr &>(*this));
        break;
    case CallExpr:
        traverser.visit(static_cast<ast::CallExpr &>(*this));
        break;
    case Constant:
        traverser.visit(static_cast<ast::Constant &>(*this));
        break;
    case Symbol:
        traverser.visit(static_cast<ast::Symbol &>(*this));
        break;
    case UnaryExpr:
        traverser.visit(static_cast<ast::UnaryExpr &>(*this));
        break;
    }
}

void Node::traverse(Traverser<TraverseOrder::PreOrder> &traverser) {
    switch (m_kind) {
        using enum NodeKind;
    case Root: {
        auto &root = static_cast<ast::Root &>(*this);
        traverser.visit(root);
        for (auto *node : root.top_level_nodes()) {
            node->traverse(traverser);
        }
        break;
    }
    case FunctionDecl:
        traverser.visit(static_cast<ast::FunctionDecl &>(*this));
        break;
    case PipelineDecl:
        traverser.visit(static_cast<ast::PipelineDecl &>(*this));
        break;
    case DeclStmt: {
        auto &stmt = static_cast<ast::DeclStmt &>(*this);
        traverser.visit(stmt);
        stmt.value().traverse(traverser);
        break;
    }
    case ReturnStmt: {
        auto &stmt = static_cast<ast::ReturnStmt &>(*this);
        traverser.visit(stmt);
        stmt.expr().traverse(traverser);
        break;
    }
    case Aggregate:
        traverser.visit(static_cast<ast::Aggregate &>(*this));
        break;
    case BinaryExpr: {
        auto &expr = static_cast<ast::BinaryExpr &>(*this);
        traverser.visit(expr);
        expr.lhs().traverse(traverser);
        expr.rhs().traverse(traverser);
        break;
    }
    case CallExpr:
        traverser.visit(static_cast<ast::CallExpr &>(*this));
        break;
    case Constant:
        traverser.visit(static_cast<ast::Constant &>(*this));
        break;
    case Symbol:
        traverser.visit(static_cast<ast::Symbol &>(*this));
        break;
    case UnaryExpr: {
        auto &expr = static_cast<ast::UnaryExpr &>(*this);
        traverser.visit(expr);
        expr.expr().traverse(traverser);
        break;
    }
    }
}

void Node::traverse(Traverser<TraverseOrder::PostOrder> &traverser) {
    switch (m_kind) {
        using enum NodeKind;
    case Root: {
        auto &root = static_cast<ast::Root &>(*this);
        for (auto *node : root.top_level_nodes()) {
            node->traverse(traverser);
        }
        traverser.visit(root);
        break;
    }
    case FunctionDecl:
        traverser.visit(static_cast<ast::FunctionDecl &>(*this));
        break;
    case PipelineDecl:
        traverser.visit(static_cast<ast::PipelineDecl &>(*this));
        break;
    case DeclStmt: {
        auto &stmt = static_cast<ast::DeclStmt &>(*this);
        stmt.value().traverse(traverser);
        traverser.visit(stmt);
        break;
    }
    case ReturnStmt: {
        auto &stmt = static_cast<ast::ReturnStmt &>(*this);
        stmt.expr().traverse(traverser);
        traverser.visit(stmt);
        break;
    }
    case Aggregate:
        traverser.visit(static_cast<ast::Aggregate &>(*this));
        break;
    case BinaryExpr: {
        auto &expr = static_cast<ast::BinaryExpr &>(*this);
        expr.lhs().traverse(traverser);
        expr.rhs().traverse(traverser);
        traverser.visit(expr);
        break;
    }
    case CallExpr:
        traverser.visit(static_cast<ast::CallExpr &>(*this));
        break;
    case Constant:
        traverser.visit(static_cast<ast::Constant &>(*this));
        break;
    case Symbol:
        traverser.visit(static_cast<ast::Symbol &>(*this));
        break;
    case UnaryExpr: {
        auto &expr = static_cast<ast::UnaryExpr &>(*this);
        expr.expr().traverse(traverser);
        traverser.visit(expr);
        break;
    }
    }
}

Root::~Root() {
    DestroyVisitor destroy_visitor(m_arena);
    traverse(destroy_visitor);
}

void Root::append_top_level(NodeHandle<Node> &&node) {
    m_top_level_nodes.push(node.disown());
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
