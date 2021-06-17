#include <vull/renderer/Fence.hh>

#include <vull/renderer/Device.hh>
#include <vull/support/Assert.hh>

#include <vulkan/vulkan_core.h>

#include <cstdint>

Fence::Fence(const Device &device, bool signalled) : m_device(&device) {
    VkFenceCreateInfo fence_ci{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = signalled ? VK_FENCE_CREATE_SIGNALED_BIT : static_cast<VkFenceCreateFlagBits>(0),
    };
    ENSURE(vkCreateFence(*device, &fence_ci, nullptr, &m_fence) == VK_SUCCESS);
}

Fence::~Fence() {
    if (m_device != nullptr) {
        vkDestroyFence(**m_device, m_fence, nullptr);
    }
}

void Fence::block(std::uint64_t timeout) const {
    ASSERT(m_device != nullptr);
    vkWaitForFences(**m_device, 1, &m_fence, VK_TRUE, timeout);
}

void Fence::reset() const {
    ASSERT(m_device != nullptr);
    vkResetFences(**m_device, 1, &m_fence);
}
