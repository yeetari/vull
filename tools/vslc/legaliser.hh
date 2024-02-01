#pragma once

#include "ast.hh"
#include "type.hh"

#include <vull/container/hash_map.hh>
#include <vull/support/string_view.hh>

class Legaliser : public ast::Traverser<ast::TraverseOrder::PostOrder> {
    // TODO: Somewhat duplicated with spv::Backend, maybe a legalised HIR would be better?
    class Scope {
        Scope *&m_current;
        Scope *m_parent;
        vull::HashMap<vull::StringView, Type> m_symbol_map;

    public:
        explicit Scope(Scope *&current);
        Scope(const Scope &) = delete;
        Scope(Scope &&) = delete;
        ~Scope();

        Scope &operator=(const Scope &) = delete;
        Scope &operator=(Scope &&) = delete;

        Type lookup_symbol(vull::StringView name) const;
        void put_symbol(vull::StringView name, Type type);
    };

    Scope *m_scope{nullptr};
    Scope m_root_scope{m_scope};

public:
    void visit(ast::Aggregate &) override;
    void visit(ast::BinaryExpr &) override;
    void visit(ast::CallExpr &) override;
    void visit(ast::Constant &) override {}
    void visit(ast::DeclStmt &) override;
    void visit(ast::Function &) override;
    void visit(ast::PipelineDecl &) override;
    void visit(ast::ReturnStmt &) override {}
    void visit(ast::Symbol &) override;
    void visit(ast::Root &) override {}
    void visit(ast::UnaryExpr &) override;
};
