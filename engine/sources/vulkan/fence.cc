#include <vull/vulkan/fence.hh>

#include <vull/support/assert.hh>
#include <vull/support/optional.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/vulkan.hh>

namespace vull::vk {

Fence::Fence(const Context &context, bool signaled) : m_context(context) {
    vkb::ExportFenceCreateInfo export_ci{
        .sType = vkb::StructureType::ExportFenceCreateInfo,
        .handleTypes = vkb::ExternalFenceHandleTypeFlags::SyncFd,
    };
    vkb::FenceCreateInfo fence_ci{
        .sType = vkb::StructureType::FenceCreateInfo,
        .pNext = &export_ci,
        .flags = signaled ? vkb::FenceCreateFlags::Signaled : vkb::FenceCreateFlags::None,
    };
    VULL_ENSURE(context.vkCreateFence(&fence_ci, &m_fence) == vkb::Result::Success);
}

Fence::~Fence() {
    m_context.vkDestroyFence(m_fence);
}

Optional<int> Fence::make_fd() const {
    vkb::FenceGetFdInfoKHR get_fd_info{
        .sType = vkb::StructureType::FenceGetFdInfoKHR,
        .fence = m_fence,
        .handleType = vkb::ExternalFenceHandleTypeFlags::SyncFd,
    };
    int fd = -1;
    VULL_ENSURE(m_context.vkGetFenceFdKHR(&get_fd_info, &fd) == vkb::Result::Success);
    return fd != -1 ? Optional<int>(fd) : vull::nullopt;
}

void Fence::reset() const {
    m_context.vkResetFences(1, &m_fence);
}

void Fence::wait(uint64_t timeout) const {
    m_context.vkWaitForFences(1, &m_fence, true, timeout);
}

} // namespace vull::vk
