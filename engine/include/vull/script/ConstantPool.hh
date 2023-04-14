#pragma once

#include <vull/container/Vector.hh>
#include <vull/script/Value.hh>

#include <stdint.h>

namespace vull::script {

class ConstantPool {
    Vector<Value, uint16_t> m_constants;

public:
    Value operator[](uint16_t index);
    uint16_t put(Value value);
};

} // namespace vull::script
