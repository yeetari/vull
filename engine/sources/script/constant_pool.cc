#include <vull/script/constant_pool.hh>

#include <vull/container/vector.hh>
#include <vull/script/value.hh>

namespace vull::script {

Value ConstantPool::operator[](uint16_t index) {
    return m_constants[index];
}

uint16_t ConstantPool::put(Value value) {
    m_constants.push(value);
    return m_constants.size() - 1;
}

} // namespace vull::script
