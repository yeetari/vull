#include <vull/shaderc/hir.hh>

#include <vull/container/vector.hh>
#include <vull/shaderc/tree.hh>
#include <vull/shaderc/type.hh>
#include <vull/support/assert.hh>
#include <vull/support/enum.hh>
#include <vull/support/optional.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>
#include <vull/support/variant.hh>

#include <stdint.h>

namespace vull::shaderc::hir {
namespace {

class Indent {
    uint32_t &m_indent;

public:
    explicit Indent(uint32_t &indent) : m_indent(indent) { indent++; }
    Indent(const Indent &) = delete;
    Indent(Indent &&) = delete;
    ~Indent() { m_indent--; }

    Indent &operator=(const Indent &) = delete;
    Indent &operator=(Indent &&) = delete;
};

class Dumper {
    StringBuilder &m_builder;
    uint32_t m_indent{0};

    template <typename... Args>
    void append(StringView fmt, const Args &...args) {
        for (uint32_t i = 0; i < m_indent; i++) {
            m_builder.append("  ");
        }
        m_builder.append(fmt, args...);
        m_builder.append('\n');
    }

public:
    explicit Dumper(StringBuilder &builder) : m_builder(builder) {}

    void visit(const PipelineVariable &pipeline_variable);

    void visit(const BinaryExpr &binary_expr);
    void visit(const Constant &constant);
    void visit(const ConstructExpr &construct_expr);
    void visit(const UnaryExpr &unary_expr);

    void visit(const ExprStmt &expr_stmt);
    void visit(const ReturnStmt &return_stmt);

    void visit(const Aggregate &aggregate);
    void visit(const FunctionDecl &function_decl);
    void visit(const Node &node);
    void visit(const Root &root);
};

StringView type_string(ScalarType scalar_type) {
    switch (scalar_type) {
    case ScalarType::Float:
        return "float";
    case ScalarType::Int:
        return "int";
    case ScalarType::Uint:
        return "uint";
    case ScalarType::Void:
        return "void";
    default:
        return "<invalid>";
    }
}

String type_string(Type type) {
    if (type.is_matrix()) {
        return vull::format("mat({}, {}, {})", type_string(type.scalar_type()), type.matrix_cols(), type.matrix_rows());
    }
    if (type.is_vector()) {
        return vull::format("vec({}, {})", type_string(type.scalar_type()), type.vector_size());
    }
    return type_string(type.scalar_type());
}

void Dumper::visit(const PipelineVariable &pipeline_variable) {
    const auto &index = pipeline_variable.index();
    if (auto special = index.try_get<SpecialPipelineVariable>()) {
        append("PipelineVariable({}, output: {})", vull::enum_name(*special), pipeline_variable.is_output());
    } else {
        append("PipelineVariable({}, output: {})", index.get<uint32_t>(), pipeline_variable.is_output());
    }
}

void Dumper::visit(const BinaryExpr &binary_expr) {
    append("BinaryExpr({}, {}, assign: {})", type_string(binary_expr.type()), vull::enum_name(binary_expr.op()),
           binary_expr.is_assign());
    Indent indent(m_indent);
    visit(binary_expr.lhs());
    visit(binary_expr.rhs());
}

void Dumper::visit(const Constant &constant) {
    append("Constant({}, {})", type_string(constant.type()), constant.value());
}

void Dumper::visit(const ConstructExpr &construct_expr) {
    append("ConstructExpr({})", type_string(construct_expr.type()));
    Indent indent(m_indent);
    for (const auto &value : construct_expr.values()) {
        visit(*value);
    }
}

void Dumper::visit(const UnaryExpr &unary_expr) {
    append("UnaryExpr({}, {})", type_string(unary_expr.type()), vull::enum_name(unary_expr.op()));
    Indent indent(m_indent);
    visit(unary_expr.expr());
}

void Dumper::visit(const ExprStmt &expr_stmt) {
    append("ExprStmt");
    Indent indent(m_indent);
    visit(expr_stmt.expr());
}

void Dumper::visit(const ReturnStmt &return_stmt) {
    append("ReturnStmt");
    Indent indent(m_indent);
    visit(return_stmt.expr());
}

void Dumper::visit(const Aggregate &aggregate) {
    append("Block");
    Indent indent(m_indent);
    for (const auto &node : aggregate.nodes()) {
        visit(*node);
    }
}

void Dumper::visit(const FunctionDecl &function_decl) {
    append("FunctionDecl {}", type_string(function_decl.return_type()));
    Indent indent(m_indent);
    if (function_decl.is_special_function(SpecialFunction::VertexEntry)) {
        append("# vertex entry");
    } else if (function_decl.is_special_function(SpecialFunction::FragmentEntry)) {
        append("# fragment entry");
    }
    visit(function_decl.body());
}

void Dumper::visit(const Node &node) {
    switch (node.kind()) {
    case NodeKind::FunctionDecl:
        visit(static_cast<const FunctionDecl &>(node));
        break;
    case NodeKind::BinaryExpr:
        visit(static_cast<const BinaryExpr &>(node));
        break;
    case NodeKind::Constant:
        visit(static_cast<const Constant &>(node));
        break;
    case NodeKind::ConstructExpr:
        visit(static_cast<const ConstructExpr &>(node));
        break;
    case NodeKind::UnaryExpr:
        visit(static_cast<const UnaryExpr &>(node));
        break;
    case NodeKind::ExprStmt:
        visit(static_cast<const ExprStmt &>(node));
        break;
    case NodeKind::ReturnStmt:
        visit(static_cast<const ReturnStmt &>(node));
        break;
    case NodeKind::Argument:
        append("Argument({h})", vull::bit_cast<uintptr_t>(&node));
        break;
    case NodeKind::LocalVariable:
        append("LocalVariable({h})", vull::bit_cast<uintptr_t>(&node));
        break;
    case NodeKind::PipelineVariable:
        visit(static_cast<const PipelineVariable &>(node));
        break;
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

void Dumper::visit(const Root &root) {
    append("Root");
    Indent indent(m_indent);
    for (const auto &node : root.top_level_nodes()) {
        visit(*node);
    }
}

} // namespace

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
    case LocalVariable:
        this->~Node();
        break;
    }
}

void Root::append_top_level(NodeHandle<Node> &&node) {
    m_top_level_nodes.push(vull::move(node));
}

void dump(const Root &root, StringBuilder &builder) {
    Dumper dumper(builder);
    dumper.visit(root);
}

} // namespace vull::shaderc::hir
