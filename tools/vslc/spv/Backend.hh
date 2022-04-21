#pragma once

#include "../Ast.hh"
#include "Builder.hh"

#include <vull/support/Vector.hh>

namespace spv {

class Backend : public ast::Traverser<ast::TraverseOrder::PostOrder> {
    // Extend from Type to allow for nicer access to type-related functions.
    class Value : public Type {
        const Id m_id;
        const Op m_creator_op;
        const vull::Vector<Word> &m_operands;

    public:
        Value(Instruction &inst, Type type)
            : Type(type), m_id(inst.id()), m_creator_op(inst.op()), m_operands(inst.operands()) {}

        Id id() const { return m_id; }
        Op creator_op() const { return m_creator_op; }
        const vull::Vector<Word> &operands() const { return m_operands; }
    };

    class Scope {
        Scope *&m_current;
        Scope *m_parent;
        struct Symbol {
            vull::StringView name;
            Id id;
        };
        // TODO(hash-map)
        vull::Vector<Symbol> m_symbol_map;

    public:
        explicit Scope(Scope *&current);
        Scope(const Scope &) = delete;
        Scope(Scope &&) = delete;
        ~Scope();

        Scope &operator=(const Scope &) = delete;
        Scope &operator=(Scope &&) = delete;

        Id lookup_symbol(vull::StringView name) const;
        void put_symbol(vull::StringView name, Id id);
    };

    Builder m_builder;
    Function *m_function{nullptr};
    Block *m_block{nullptr};
    Scope *m_scope{nullptr};
    vull::Vector<Value> m_value_stack;

    // Vertex shader.
    Id m_position_output{0};
    bool m_is_vertex_entry{false};

    Id convert_type(ScalarType);
    Id convert_type(const Type &);

    Instruction &translate_construct_expr(const Type &);

public:
    void visit(ast::Aggregate &) override;
    void visit(ast::BinaryExpr &) override;
    void visit(ast::Constant &) override;
    void visit(ast::DeclStmt &) override;
    void visit(ast::Function &) override;
    void visit(ast::ReturnStmt &) override;
    void visit(ast::Symbol &) override;
    void visit(ast::Root &) override {}
    void visit(ast::UnaryExpr &) override;

    const Builder &builder() const { return m_builder; }
};

} // namespace spv
