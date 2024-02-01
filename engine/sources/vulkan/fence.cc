#include <vull/vulkan/fence.hh>

#include <vull/support/assert.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/vulkan.hh>

namespace vull::vk {

Fence::Fence(const Context &context, bool signaled) : m_context(context) {
    vkb::FenceCreateInfo fence_ci{
        .sType = vkb::StructureType::FenceCreateInfo,
        .flags = signaled ? vkb::FenceCreateFlags::Signaled : vkb::FenceCreateFlags::None,
    };
    VULL_ENSURE(context.vkCreateFence(&fence_ci, &m_fence) == vkb::Result::Success);
}

Fence::~Fence() {
    m_context.vkDestroyFence(m_fence);
}

void Fence::reset() const {
    m_context.vkResetFences(1, &m_fence);
}

void Fence::wait(uint64_t timeout) const {
    m_context.vkWaitForFences(1, &m_fence, true, timeout);
}

} // namespace vull::vk
