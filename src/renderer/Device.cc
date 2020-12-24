#include <renderer/Device.hh>

#include <support/Assert.hh>

namespace {

constexpr const char *SWAPCHAIN_EXTENSION_NAME = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

} // namespace

Device::Device(VkPhysicalDevice physical) : m_physical(physical) {
    std::uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &queue_family_count, nullptr);
    m_queue_families.relength(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &queue_family_count, m_queue_families.data());
    Vector<VkDeviceQueueCreateInfo> queue_cis;
    for (std::uint32_t i = 0; const auto &queue_family : m_queue_families) {
        const float priority = 1.0F;
        VkDeviceQueueCreateInfo queue_ci{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = i++,
            .queueCount = 1,
            .pQueuePriorities = &priority,
        };
        queue_cis.push(queue_ci);
    }
    VkPhysicalDeviceFeatures device_features{
        .samplerAnisotropy = VK_TRUE,
    };
    VkDeviceCreateInfo device_ci{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = queue_cis.length(),
        .pQueueCreateInfos = queue_cis.data(),
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = &SWAPCHAIN_EXTENSION_NAME,
        .pEnabledFeatures = &device_features,
    };
    ENSURE(vkCreateDevice(physical, &device_ci, nullptr, &m_device) == VK_SUCCESS);
}

Device::~Device() {
    vkDestroyDevice(m_device, nullptr);
}
