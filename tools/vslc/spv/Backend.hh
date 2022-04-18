#pragma once

#include "../Ast.hh"
#include "Builder.hh"

#include <vull/support/Vector.hh>

namespace spv {

class Backend : public ast::Visitor {
    class Value {
        const Id m_id;
        const Op m_creator_op;
        const vull::Vector<Word> &m_operands;
        // TODO: If Builder had a way to retrieve a type from an ID we wouldn't have to store the AST type.
        const ast::Type m_vsl_type;

    public:
        Value(Instruction &inst, const ast::Type &vsl_type)
            : m_id(inst.id()), m_creator_op(inst.op()), m_operands(inst.operands()), m_vsl_type(vsl_type) {}

        Id id() const { return m_id; }
        Op creator_op() const { return m_creator_op; }
        const vull::Vector<Word> &operands() const { return m_operands; }
        const ast::Type &vsl_type() const { return m_vsl_type; }
    };

    class Scope {
        Scope *&m_current;
        Scope *m_parent;
        struct Symbol {
            vull::StringView name;
            Id id;
            const ast::Type &vsl_type;
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

        const Symbol &lookup_symbol(vull::StringView name) const;
        void put_symbol(vull::StringView name, Id id, const ast::Type &vsl_type);
    };

    Builder m_builder;
    Function *m_function{nullptr};
    Block *m_block{nullptr};
    Scope *m_scope{nullptr};
    vull::Vector<Value> m_value_stack;

    // Vertex shader.
    Id m_position_output{0};
    bool m_is_vertex_entry{false};

    Id convert_type(ast::ScalarType);
    Id convert_type(const ast::Type &);

    Instruction &translate_construct_expr(const ast::Type &);

public:
    void visit(const ast::Aggregate &) override;
    void visit(const ast::Constant &) override;
    void visit(const ast::Function &) override;
    void visit(const ast::ReturnStmt &) override;
    void visit(const ast::Symbol &) override;

    const Builder &builder() const { return m_builder; }
};

} // namespace spv
