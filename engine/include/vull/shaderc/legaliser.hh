#pragma once

#include <vull/container/hash_map.hh>
#include <vull/shaderc/ast.hh>
#include <vull/shaderc/hir.hh>
#include <vull/support/result.hh>
#include <vull/support/tuple.hh>
#include <vull/support/unique_ptr.hh>

namespace vull::shaderc {

enum class LegaliseError {

};

class Legaliser {
    class Scope;

private:
    hir::Root m_root;
    Scope *m_scope{nullptr};
    UniquePtr<Scope> m_root_scope;
    HashMap<StringView, Tuple<hir::BuiltinFunction, Type>> m_builtin_functions;

    Result<hir::Expr *, LegaliseError> lower_binary_expr(const ast::BinaryExpr &);
    Result<hir::Expr *, LegaliseError> lower_call_expr(const ast::CallExpr &);
    Result<hir::Expr *, LegaliseError> lower_symbol(const ast::Symbol &);
    Result<hir::Expr *, LegaliseError> lower_expr(const ast::Node &);

    Result<hir::Node *, LegaliseError> lower_function_decl(const ast::FunctionDecl &);
    Result<hir::Node *, LegaliseError> lower_pipeline_decl(const ast::PipelineDecl &);
    Result<hir::Node *, LegaliseError> lower_top_level(const ast::Node &);

public:
    Legaliser();
    Legaliser(const Legaliser &) = delete;
    Legaliser(Legaliser &&) = delete;
    ~Legaliser();

    Legaliser &operator=(const Legaliser &) = delete;
    Legaliser &operator=(Legaliser &&) = delete;

    Result<hir::Root, LegaliseError> legalise(const ast::Root &);
};

} // namespace vull::shaderc
