#pragma once

#include <vull/container/hash_map.hh>
#include <vull/shaderc/ast.hh>
#include <vull/shaderc/hir.hh>
#include <vull/shaderc/type.hh>
#include <vull/support/result.hh>
#include <vull/support/string_view.hh>
#include <vull/support/tuple.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>

namespace vull::shaderc {

enum class LegaliseError {

};

template <typename T>
using LegaliseResult = Result<hir::NodeHandle<T>, LegaliseError>;

class Legaliser {
    class Scope;

private:
    hir::Root &m_root;
    Scope *m_scope{nullptr};
    UniquePtr<Scope> m_root_scope;
    HashMap<StringView, Tuple<hir::BuiltinFunction, Type>> m_builtin_functions;
    Vector<const ast::IoDecl &> m_pipeline_decls;

    LegaliseResult<hir::Expr> lower_binary_expr(const ast::BinaryExpr &);
    LegaliseResult<hir::Expr> lower_call_expr(const ast::CallExpr &);
    LegaliseResult<hir::Expr> lower_constant(const ast::Constant &);
    LegaliseResult<hir::Expr> lower_construct_expr(const ast::Aggregate &);
    LegaliseResult<hir::Expr> lower_symbol(const ast::Symbol &);
    LegaliseResult<hir::Expr> lower_expr(const ast::Node &);

    LegaliseResult<hir::Node> lower_decl_stmt(const ast::DeclStmt &);
    LegaliseResult<hir::Node> lower_return_stmt(const ast::ReturnStmt &);
    LegaliseResult<hir::Node> lower_stmt(const ast::Node &);
    LegaliseResult<hir::Aggregate> lower_block(const ast::Aggregate &);

    LegaliseResult<hir::FunctionDecl> lower_function_decl(const ast::FunctionDecl &);
    Result<void, LegaliseError> lower_io_decl(const ast::IoDecl &);
    // Result<void, LegaliseError> lower_pipeline_decl(const ast::PipelineDecl &);
    // Result<void, LegaliseError> lower_uniform_decl(const ast::Aggregate &);
    Result<void, LegaliseError> lower_top_level(const ast::Node &);

public:
    explicit Legaliser(hir::Root &root);
    Legaliser(const Legaliser &) = delete;
    Legaliser(Legaliser &&) = delete;
    ~Legaliser();

    Legaliser &operator=(const Legaliser &) = delete;
    Legaliser &operator=(Legaliser &&) = delete;

    Result<void, LegaliseError> legalise(const ast::Root &);
};

} // namespace vull::shaderc
