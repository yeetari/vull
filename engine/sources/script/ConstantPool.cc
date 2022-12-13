#include <vull/script/ConstantPool.hh>

#include <vull/script/Value.hh>
#include <vull/support/Vector.hh>

namespace vull::script {

Value ConstantPool::operator[](uint16_t index) {
    return m_constants[index];
}

uint16_t ConstantPool::put(Value value) {
    m_constants.push(value);
    return m_constants.size() - 1;
}

} // namespace vull::script
