#include <vull/vulkan/semaphore.hh>

#include <vull/support/assert.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/vulkan.hh>

namespace vull::vk {

Semaphore::Semaphore(const Context &context) : m_context(context) {
    vkb::SemaphoreCreateInfo semaphore_ci{
        .sType = vkb::StructureType::SemaphoreCreateInfo,
    };
    VULL_ENSURE(m_context.vkCreateSemaphore(&semaphore_ci, &m_semaphore) == vkb::Result::Success);
}

Semaphore::~Semaphore() {
    m_context.vkDestroySemaphore(m_semaphore);
}

} // namespace vull::vk
