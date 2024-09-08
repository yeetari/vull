#include <vull/shaderc/tree.hh>

#include <vull/maths/common.hh>
#include <vull/support/assert.hh>
#include <vull/support/utility.hh>

#include <stddef.h>

namespace vull::shaderc::tree {

void *ArenaChunk::allocate(size_t size, size_t alignment) {
    size = vull::align_up(size, alignment);
    VULL_ASSERT(size <= k_size);
    if (m_head + size >= k_size) {
        return nullptr;
    }
    return m_data + vull::exchange(m_head, m_head + size);
}

void NodeBase::add_ref() {
    m_ref_count++;
}

bool NodeBase::sub_ref() {
    VULL_ASSERT(m_ref_count >= 1);
    m_ref_count--;
    return m_ref_count == 0;
}

} // namespace vull::shaderc::tree
