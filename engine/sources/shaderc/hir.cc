#include <vull/shaderc/hir.hh>

namespace vull::shaderc::hir {

void Node::destroy() {
    switch (m_kind) {
        using enum NodeKind;
    case FunctionDecl:
        static_cast<hir::FunctionDecl *>(this)->~FunctionDecl();
        break;
    case Block:
        static_cast<hir::Aggregate *>(this)->~Aggregate();
        break;
    case ExprStmt:
        static_cast<hir::ExprStmt *>(this)->~ExprStmt();
        break;
    case ReturnStmt:
        static_cast<hir::ReturnStmt *>(this)->~ReturnStmt();
        break;
    case BinaryExpr:
        static_cast<hir::BinaryExpr *>(this)->~BinaryExpr();
        break;
    case BuiltinExpr:
        static_cast<hir::BuiltinExpr *>(this)->~BuiltinExpr();
        break;
    case Constant:
        static_cast<hir::Constant *>(this)->~Constant();
        break;
    case ConstructExpr:
        static_cast<hir::ConstructExpr *>(this)->~ConstructExpr();
        break;
    case UnaryExpr:
        static_cast<hir::UnaryExpr *>(this)->~UnaryExpr();
        break;
    case PipelineVariable:
        static_cast<hir::PipelineVariable *>(this)->~PipelineVariable();
        break;
    case Argument:
    case LocalVar:
        this->~Node();
        break;
    }
}

void Root::append_top_level(NodeHandle<Node> &&node) {
    m_top_level_nodes.push(vull::move(node));
}

} // namespace vull::shaderc::hir
