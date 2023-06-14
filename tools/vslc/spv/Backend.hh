#pragma once

#include "../Ast.hh"
#include "../Type.hh"
#include "Builder.hh"
#include "Spirv.hh"

#include <vull/container/HashMap.hh>
#include <vull/container/Vector.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Span.hh>
#include <vull/support/StringView.hh>

#include <stdint.h>

namespace spv {

class Backend : public ast::Traverser<ast::TraverseOrder::None> {
    // Extend from Type to allow for nicer access to type-related functions.
    class Value : public Type {
        const Id m_id;
        const Op m_creator_op;
        const vull::Span<const Word> m_operands;

    public:
        Value(Id id, Type type) : Type(type), m_id(id), m_creator_op{} {}

        Value(Instruction &inst, Type type)
            : Type(type), m_id(inst.id()), m_creator_op(inst.op()), m_operands(inst.operands().span()) {}

        Id id() const { return m_id; }
        Op creator_op() const { return m_creator_op; }
        vull::Span<const Word> operands() const { return m_operands; }
    };

    struct Symbol {
        Id id;
        vull::Optional<uint8_t> uniform_index;
    };

    class Scope {
        Scope *&m_current;
        Scope *m_parent;
        vull::HashMap<vull::StringView, Symbol> m_symbol_map;

    public:
        explicit Scope(Scope *&current);
        Scope(const Scope &) = delete;
        Scope(Scope &&) = delete;
        ~Scope();

        Scope &operator=(const Scope &) = delete;
        Scope &operator=(Scope &&) = delete;

        const Symbol &lookup_symbol(vull::StringView name) const;
        Symbol &put_symbol(vull::StringView name, Id id);
    };

    Builder m_builder;
    Function *m_function{nullptr};
    Block *m_block{nullptr};
    Scope *m_scope{nullptr};
    Scope m_root_scope{m_scope};
    vull::Vector<Value> m_value_stack;
    Id m_std_450{};

    Id m_fragment_output_id{};
    bool m_is_fragment_entry{false};

    uint8_t m_pipeline_variable_counter{0};
    bool m_load_symbol{true};

    Id convert_type(ScalarType);
    Id convert_type(const Type &);

    Instruction &translate_construct_expr(const Type &);

public:
    void visit(ast::Aggregate &) override;
    void visit(ast::BinaryExpr &) override;
    void visit(ast::CallExpr &) override;
    void visit(ast::Constant &) override;
    void visit(ast::DeclStmt &) override;
    void visit(ast::Function &) override;
    void visit(ast::PipelineDecl &) override;
    void visit(ast::ReturnStmt &) override;
    void visit(ast::Symbol &) override;
    void visit(ast::Root &) override;
    void visit(ast::UnaryExpr &) override;

    const Builder &builder() const { return m_builder; }
};

} // namespace spv
