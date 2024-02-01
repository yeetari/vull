#pragma once

#include <vull/script/constant_pool.hh>
#include <vull/script/value.hh>
#include <vull/support/utility.hh>

namespace vull::script {

class Frame;

class Vm {
    ConstantPool m_constant_pool;

public:
    explicit Vm(ConstantPool &&constant_pool) : m_constant_pool(vull::move(constant_pool)) {}

    void dump_frame(Frame &frame);
    Value exec_frame(Frame &frame);
};

} // namespace vull::script
