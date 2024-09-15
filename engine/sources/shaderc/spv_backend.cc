#include <vull/shaderc/spv_backend.hh>

#include <vull/container/hash_map.hh>
#include <vull/shaderc/hir.hh>
#include <vull/shaderc/spv_builder.hh>
#include <vull/support/assert.hh>
#include <vull/support/tuple.hh>
#include <vull/vulkan/spirv.hh>

namespace vull::shaderc::spv {
namespace {

class Value {
    Id m_id{};
    Id m_type_id{};
    Op m_creator_op{};
    Span<const Word> m_operands;

    Value(Id id, Id type_id, Op creator_op, Span<const Word> operands)
        : m_id(id), m_type_id(type_id), m_creator_op(creator_op), m_operands(operands) {}

public:
    static Value make(const Instruction &inst) { return {inst.id(), inst.type(), inst.op(), inst.operands().span()}; }

    Value() = default;

    bool is_null() const { return m_id == 0; }

    Id id() const { return m_id; }
    Id type_id() const { return m_type_id; }
    Op creator_op() const { return m_creator_op; }
    Span<const Word> operands() const { return m_operands; }
};

class AccessChain {
    Value m_base_value;
    bool m_is_rvalue{false};

    AccessChain(const Value &base_value, bool is_rvalue) : m_base_value(base_value), m_is_rvalue(is_rvalue) {}

public:
    static AccessChain from_lvalue(const Value &base_value) { return {base_value, false}; }
    static AccessChain from_rvalue(const Value &base_value) { return {base_value, true}; }

    const Value &base_value() const { return m_base_value; }
    bool is_rvalue() const { return m_is_rvalue; }
};

class Backend {
    Builder &m_builder;
    Id m_std_450{};

    Function *m_function{nullptr};
    Block *m_block{nullptr};

    // Only valid if in an entry-point defining function.
    EntryPoint *m_entry_point{nullptr};

    // Cache of materialised variables.
    HashMap<const hir::Expr *, Value> m_variable_map;

    // AccessChain m_fragment_output_chain;
    bool m_is_fragment_entry{false};

private:
    Id lower_type(ScalarType scalar_type);
    Id lower_type(const Type &vsl_type);

    Value flatten_access_chain(const AccessChain &chain);
    Value load_access_chain(const AccessChain &chain);
    void store_access_chain(const AccessChain &chain, Value rvalue);

    AccessChain lower_binary_expr(const hir::BinaryExpr &);

    Tuple<Op, Id, Word> lower_builtin_function(hir::BuiltinFunction function) const;
    Value lower_builtin_expr(const hir::BuiltinExpr *builtin_expr);

    AccessChain lower_constant(const hir::Constant &);
    AccessChain lower_construct_expr(const hir::ConstructExpr &);

    Value lower_unary_expr(const hir::UnaryExpr *unary_expr);

    Value materialise_variable(const hir::Expr &);
    AccessChain lower_variable(const hir::Expr &);

    AccessChain lower_expr(const hir::Expr &);

    void lower_return_stmt(const hir::ReturnStmt *return_stmt);
    void lower_stmt(const hir::Node &);
    void lower_block(const hir::Aggregate &);
    void lower_function_decl(const hir::FunctionDecl &);

public:
    explicit Backend(Builder &builder);

    void lower_top_level(const hir::Node &node);
};

Backend::Backend(Builder &builder) : m_builder(builder) {
    // TODO: Use vulkan memory model?
    builder.set_memory_model(AddressingModel::PhysicalStorageBuffer64, MemoryModel::Glsl450);
    m_std_450 = builder.import_extension("GLSL.std.450");
}

Id Backend::lower_type(ScalarType scalar_type) {
    switch (scalar_type) {
    case ScalarType::Void:
        return m_builder.void_type();
    case ScalarType::Float:
        return m_builder.float_type(32);
    case ScalarType::Int:
        return m_builder.int_type(32, true);
    case ScalarType::Uint:
        return m_builder.int_type(32, false);
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

Id Backend::lower_type(const Type &vsl_type) {
    const auto scalar_type = lower_type(vsl_type.scalar_type());
    if (vsl_type.is_scalar()) {
        return scalar_type;
    }
    const auto vector_type = m_builder.vector_type(scalar_type, vsl_type.vector_size());
    if (vsl_type.is_vector()) {
        return vector_type;
    }
    return m_builder.matrix_type(vector_type, vsl_type.matrix_cols());
}

Value Backend::flatten_access_chain(const AccessChain &chain) {
    return chain.base_value();
}

Value Backend::load_access_chain(const AccessChain &chain) {
    if (chain.is_rvalue()) {
        // Already an rvalue.
        return chain.base_value();
    }

    Id type_id = chain.base_value().type_id();
    type_id = m_builder.inner_type(type_id);

    auto &load_inst = m_block->append(Op::Load, type_id);
    load_inst.append_operand(chain.base_value().id());
    return Value::make(load_inst);
}

void Backend::store_access_chain(const AccessChain &chain, Value rvalue) {
    // TODO: Optimise initial constant stores to variables.
    VULL_ASSERT(!chain.is_rvalue());
    auto &store_inst = m_block->append(Op::Store);
    store_inst.append_operand(chain.base_value().id());
    store_inst.append_operand(rvalue.id());
}

Op lower_binary_op(hir::BinaryOp op) {
    switch (op) {
        using enum hir::BinaryOp;
    case Add:
        return Op::FAdd;
    case Sub:
        return Op::FSub;
    case Div:
        return Op::FDiv;
    case ScalarTimesScalar:
    case VectorTimesVector:
        return Op::FMul;
    case VectorTimesScalar:
        return Op::VectorTimesScalar;
    case MatrixTimesScalar:
        return Op::MatrixTimesScalar;
    case VectorTimesMatrix:
        return Op::VectorTimesMatrix;
    case MatrixTimesVector:
        return Op::MatrixTimesVector;
    case MatrixTimesMatrix:
        return Op::MatrixTimesMatrix;
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

AccessChain Backend::lower_binary_expr(const hir::BinaryExpr &binary_expr) {
    const auto lhs_chain = lower_expr(binary_expr.lhs());
    const auto rhs_chain = lower_expr(binary_expr.rhs());

    // Evaluate both sides as rvalues.
    Value lhs;
    if (binary_expr.op() != hir::BinaryOp::Assign) {
        lhs = load_access_chain(lhs_chain);
    }
    Value rhs = load_access_chain(rhs_chain);

    // Create operation instruction if not a simple assign.
    if (binary_expr.op() != hir::BinaryOp::Assign) {
        auto &inst = m_block->append(lower_binary_op(binary_expr.op()), lower_type(binary_expr.type()));
        inst.append_operand(lhs.id());
        inst.append_operand(rhs.id());
        rhs = Value::make(inst);
    }

    if (binary_expr.is_assign()) {
        store_access_chain(lhs_chain, rhs);
        return lhs_chain;
    }

    // Non-assignment binary expressions create a new rvalue access chain.
    return AccessChain::from_rvalue(rhs);
}

// Tuple<Op, Id, Word> Backend::lower_builtin_function(hir::BuiltinFunction function) const {
//     switch (function) {
//         using enum hir::BuiltinFunction;
//     case Dot:
//         return vull::make_tuple(Op::Dot, 0u, 0u);
//     case Max:
//         return vull::make_tuple(Op::ExtInst, m_std_450, 40u);
//     }
// }

// Value Backend::lower_builtin_expr(const hir::BuiltinExpr *builtin_expr) {
//     // TODO
//     Vector<Id> argument_ids;

//     const auto [op, set_id, set_inst] = lower_builtin_function(builtin_expr->function());
//     auto &inst = m_block->append(op, lower_type(builtin_expr->type()));
//     if (op == Op::ExtInst) {
//         inst.append_operand(set_id);
//         inst.append_operand(set_inst);
//     }
//     inst.extend_operands(argument_ids);
//     return {inst, builtin_expr->type()};
// }

AccessChain Backend::lower_constant(const hir::Constant &constant) {
    const auto type_id = lower_type(constant.scalar_type());
    const auto id = m_builder.scalar_constant(type_id, static_cast<Word>(constant.value()));
    return AccessChain::from_rvalue(Value::make(*m_builder.lookup_constant(id)));
}

AccessChain Backend::lower_construct_expr(const hir::ConstructExpr &construct_expr) {
    // TODO: vull::transform
    Vector<Value> values;
    for (const auto &value : construct_expr.values()) {
        values.push(load_access_chain(lower_expr(*value)));
    }

    // Break down any composites.
    Vector<Id> constituents;
    bool is_constant = true;
    for (const auto &value : values) {
        switch (value.creator_op()) {
        case Op::Constant:
            constituents.push(value.id());
            break;
        case Op::ConstantComposite:
        case Op::CompositeConstruct:
            is_constant &= value.creator_op() == Op::ConstantComposite;
            for (Id constant : value.operands()) {
                constituents.push(constant);
            }
            break;
        default: {
            // Need to extract from a dynamically-created composite.
            is_constant = false;

            const auto &type = *m_builder.lookup_type(value.type_id());
            VULL_ENSURE(type.op() == Op::TypeVector);

            const auto vector_size = type.operand(1);
            const auto scalar_type = m_builder.inner_type(value.type_id());
            for (uint32_t i = 0; i < vector_size; i++) {
                auto &extract_inst = m_block->append(Op::CompositeExtract, scalar_type);
                extract_inst.append_operand(value.id());
                extract_inst.append_operand(i);
                constituents.push(extract_inst.id());
            }
            break;
        }
        }
    }

    // Extend vec(x) to vec(x * n).
    if (constituents.size() == 1) {
        for (uint32_t i = 1; i < construct_expr.type().vector_size(); i++) {
            constituents.push(Id(constituents.first()));
        }
    }

    const auto composite_type = lower_type(construct_expr.type());
    if (is_constant) {
        const auto constant_id = m_builder.composite_constant(composite_type, vull::move(constituents));
        return AccessChain::from_rvalue(Value::make(*m_builder.lookup_constant(constant_id)));
    }

    auto &inst = m_block->append(Op::CompositeConstruct, composite_type);
    inst.extend_operands(constituents);
    return AccessChain::from_rvalue(Value::make(inst));
}

// Value Backend::lower_unary_expr(const hir::UnaryExpr *unary_expr) {
//     auto value = lower_expr(unary_expr->expr());
//     auto &inst = m_block->append(Op::FNegate, lower_type(unary_expr->type()));
//     inst.append_operand(value.id());
//     return {inst, unary_expr->type()};
// }

Value Backend::materialise_variable(const hir::Expr &expr) {
    switch (expr.kind()) {
        using enum hir::NodeKind;
    case BindingVariable: {
        Vector<Id> field_types;
        field_types.push(lower_type(expr.type()));

        // const auto struct_type = m_builder.struct_type(field_types, false);

        auto &variable = m_entry_point->append_variable(field_types[0], StorageClass::PushConstant);
        m_builder.decorate(variable.id(), Decoration::Block);
        return Value::make(variable);
    }
    case LocalVar:
        VULL_ASSERT(m_function != nullptr);
        return Value::make(m_function->append_variable(lower_type(expr.type())));
    case PipelineVariable: {
        const auto &pipeline_variable = static_cast<const hir::PipelineVariable &>(expr);
        const auto storage_class = pipeline_variable.is_output() ? StorageClass::Output : StorageClass::Input;
        auto &variable = m_entry_point->append_variable(lower_type(expr.type()), storage_class);
        if (const auto special = pipeline_variable.index().try_get<hir::SpecialPipelineVariable>()) {
            m_builder.decorate(variable.id(), Decoration::BuiltIn, BuiltIn::Position);
        } else {
            const auto index = pipeline_variable.index().get<uint32_t>();
            m_builder.decorate(variable.id(), Decoration::Location, index);
        }
        return Value::make(variable);
    }
    default:
        VULL_ENSURE_NOT_REACHED();
    }
    VULL_ENSURE_NOT_REACHED();
}

AccessChain Backend::lower_variable(const hir::Expr &expr) {
    auto value = m_variable_map.get(&expr).value_or({});
    if (value.is_null()) {
        value = materialise_variable(expr);
        m_variable_map.set(&expr, value);
    }
    return AccessChain::from_lvalue(value);
}

AccessChain Backend::lower_expr(const hir::Expr &expr) {
    switch (expr.kind()) {
        using enum hir::NodeKind;
    case BinaryExpr:
        return lower_binary_expr(static_cast<const hir::BinaryExpr &>(expr));
    case Constant:
        return lower_constant(static_cast<const hir::Constant &>(expr));
    case ConstructExpr:
        return lower_construct_expr(static_cast<const hir::ConstructExpr &>(expr));
    default:
        return lower_variable(expr);
    }
}

// void Backend::lower_return_stmt(const hir::ReturnStmt *return_stmt) {
//     Id rvalue = load_access_chain(lower_expr(return_stmt->expr()));
//     if (m_is_fragment_entry) {
//         store_access_chain(m_fragment_output_chain, rvalue);
//         return;
//     }
//     auto &return_inst = m_block->append(Op::ReturnValue);
//     return_inst.append_operand(rvalue);
// }

void Backend::lower_stmt(const hir::Node &stmt) {
    switch (stmt.kind()) {
        using enum hir::NodeKind;
    case ExprStmt:
        lower_expr(static_cast<const hir::ExprStmt &>(stmt).expr());
        break;
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

void Backend::lower_block(const hir::Aggregate &block) {
    m_block = &m_function->append_block();
    for (const auto &stmt : block.nodes()) {
        lower_stmt(*stmt);
    }
}

void Backend::lower_function_decl(const hir::FunctionDecl &function_decl) {
    const bool is_vertex_entry = function_decl.is_special_function(hir::SpecialFunction::VertexEntry);
    m_is_fragment_entry = function_decl.is_special_function(hir::SpecialFunction::FragmentEntry);

    // TODO: vull::transform
    Vector<Id> parameter_types;
    for (const auto type : function_decl.parameter_types()) {
        parameter_types.push(lower_type(type));
    }

    const auto return_type = m_builder.void_type();
    m_function = &m_builder.append_function(return_type, m_builder.function_type(return_type, {}));

    if (is_vertex_entry) {
        m_entry_point = &m_builder.append_entry_point("vertex_main", *m_function, ExecutionModel::Vertex);

    } else {
        VULL_ENSURE_NOT_REACHED();
    }

    // Lower top level block.
    lower_block(function_decl.body());
    m_entry_point = nullptr;

    // Add implicit return if needed.
    if (!m_block->is_terminated()) {
        m_block->append(Op::Return);
    }
}

void Backend::lower_top_level(const hir::Node &node) {
    switch (node.kind()) {
        using enum hir::NodeKind;
    case FunctionDecl:
        lower_function_decl(static_cast<const hir::FunctionDecl &>(node));
        break;
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

} // namespace

void build_spv(Builder &builder, const hir::Root &root) {
    Backend backend(builder);
    for (const auto &node : root.top_level_nodes()) {
        backend.lower_top_level(*node);
    }
}

} // namespace vull::shaderc::spv
