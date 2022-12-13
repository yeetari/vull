#pragma once

#include <vull/script/ConstantPool.hh>
#include <vull/script/Value.hh>
#include <vull/support/Utility.hh>

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
