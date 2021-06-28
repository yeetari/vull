#include <vull/vulkan/Semaphore.hh>

#include <vull/support/Assert.hh>
#include <vull/vulkan/Device.hh>

#include <vulkan/vulkan_core.h>

Semaphore::Semaphore(const Device &device) : m_device(&device) {
    VkSemaphoreCreateInfo semaphore_ci{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    ENSURE(vkCreateSemaphore(*device, &semaphore_ci, nullptr, &m_semaphore) == VK_SUCCESS);
}

Semaphore::~Semaphore() {
    if (m_device != nullptr) {
        vkDestroySemaphore(**m_device, m_semaphore, nullptr);
    }
}
