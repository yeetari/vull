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

NodeHandle<Node> Node::get_attribute(NodeKind kind) const {
    for (const auto &attribute : m_attributes) {
        if (attribute->kind() == kind) {
            return attribute.share();
        }
    }
    return {};
}

bool Node::has_attribute(NodeKind kind) const {
    return !get_attribute(kind).is_null();
}

void Node::destroy() {
    switch (m_kind) {
        using enum NodeKind;
    case Root:
        static_cast<ast::Root *>(this)->~Root();
        break;
    case FunctionDecl:
        static_cast<ast::FunctionDecl *>(this)->~FunctionDecl();
        break;
    case IoDecl:
        static_cast<ast::IoDecl *>(this)->~IoDecl();
        break;
    case DeclStmt:
        static_cast<ast::DeclStmt *>(this)->~DeclStmt();
        break;
    case ReturnStmt:
        static_cast<ast::ReturnStmt *>(this)->~ReturnStmt();
        break;
    case Aggregate:
        static_cast<ast::Aggregate *>(this)->~Aggregate();
        break;
    case BinaryExpr:
        static_cast<ast::BinaryExpr *>(this)->~BinaryExpr();
        break;
    case CallExpr:
        static_cast<ast::CallExpr *>(this)->~CallExpr();
        break;
    case Constant:
        static_cast<ast::Constant *>(this)->~Constant();
        break;
    case StringLit:
        static_cast<ast::StringLit *>(this)->~StringLit();
        break;
    case Symbol:
        static_cast<ast::Symbol *>(this)->~Symbol();
        break;
    case UnaryExpr:
        static_cast<ast::UnaryExpr *>(this)->~UnaryExpr();
        break;
    case PushConstant:
        static_cast<ast::Attribute *>(this)->~Attribute();
        break;
    }
}

void Node::traverse(Traverser<TraverseOrder::None> &traverser) {
    switch (m_kind) {
        using enum NodeKind;
    case Root:
        traverser.visit(static_cast<ast::Root &>(*this));
        break;
    case FunctionDecl:
        traverser.visit(static_cast<ast::FunctionDecl &>(*this));
        break;
    case IoDecl:
        traverser.visit(static_cast<ast::IoDecl &>(*this));
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
    case StringLit:
        traverser.visit(static_cast<ast::StringLit &>(*this));
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
        for (const auto &node : root.top_level_nodes()) {
            node->traverse(traverser);
        }
        break;
    }
    case FunctionDecl:
        traverser.visit(static_cast<ast::FunctionDecl &>(*this));
        break;
    case IoDecl:
        traverser.visit(static_cast<ast::IoDecl &>(*this));
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
    case StringLit:
        traverser.visit(static_cast<ast::StringLit &>(*this));
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
        for (const auto &node : root.top_level_nodes()) {
            node->traverse(traverser);
        }
        traverser.visit(root);
        break;
    }
    case FunctionDecl:
        traverser.visit(static_cast<ast::FunctionDecl &>(*this));
        break;
    case IoDecl:
        traverser.visit(static_cast<ast::IoDecl &>(*this));
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
    case StringLit:
        traverser.visit(static_cast<ast::StringLit &>(*this));
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

void Root::append_top_level(NodeHandle<Node> &&node) {
    m_top_level_nodes.push(vull::move(node));
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
    }

    m_indent++;
    for (const auto &node : aggregate.nodes()) {
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
    print(vull::format("CallExpr({}, intrinsic: {})", call_expr.name(), call_expr.is_intrinsic()));

    m_indent++;
    for (const auto &argument : call_expr.arguments()) {
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
    if (decl_stmt.has_value()) {
        m_indent++;
        decl_stmt.value().traverse(*this);
        m_indent--;
    }
}

void Dumper::visit(FunctionDecl &function_decl) {
    // TODO: Return type.
    // TODO: Parameters.
    print(vull::format("FunctionDecl({})", function_decl.name()));
    if (function_decl.has_body()) {
        m_indent++;
        function_decl.block().traverse(*this);
        m_indent--;
    }
}

void Dumper::visit(IoDecl &io_decl) {
    // TODO: Print type.
    print(vull::format("IoDecl({})", vull::enum_name(io_decl.io_kind())));
    m_indent++;
    io_decl.symbol_or_block().traverse(*this);
    m_indent--;
}

void Dumper::visit(ReturnStmt &return_stmt) {
    print("ReturnStmt");
    m_indent++;
    return_stmt.expr().traverse(*this);
    m_indent--;
}

void Dumper::visit(StringLit &string_lit) {
    print(vull::format("StringLit(\"{}\")", string_lit.value()));
}

void Dumper::visit(Symbol &symbol) {
    // TODO: Print type.
    print(vull::format("Symbol({})", symbol.name()));
}

void Dumper::visit(Root &root) {
    print("Root");
    m_indent++;
    for (const auto &node : root.top_level_nodes()) {
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
