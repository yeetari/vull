#pragma once

#include "../Ast.hh"
#include "Builder.hh"

#include <vull/support/Vector.hh>

namespace spv {

class Backend : public ast::Visitor {
    class InstRef {
        Instruction *m_inst;

    public:
        InstRef(Instruction &inst) : m_inst(&inst) {}

        operator Instruction &() { return *m_inst; }
    };

    Builder m_builder;
    Function *m_function{nullptr};
    Block *m_block{nullptr};
    vull::Vector<InstRef> m_expr_stack;

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

    const Builder &builder() const { return m_builder; }
};

} // namespace spv
