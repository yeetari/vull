#include <vull/renderer/Device.hh>

#include <vull/renderer/Instance.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Log.hh>

#include <string>

namespace {

constexpr const char *k_swapchain_extension_name = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

const char *device_type(VkPhysicalDeviceType type) {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return "integrated";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return "discrete";
    default:
        return "other";
    }
}

const char *queue_flag(VkQueueFlags flag) {
    switch (flag) {
    case VK_QUEUE_GRAPHICS_BIT:
        return "G";
    case VK_QUEUE_COMPUTE_BIT:
        return "C";
    case VK_QUEUE_TRANSFER_BIT:
        return "T";
    case VK_QUEUE_SPARSE_BINDING_BIT:
        return "SB";
    default:
        return "unknown";
    }
}

} // namespace

Device::Device(const Instance &instance, VkPhysicalDevice physical) : m_physical(physical) {
    VkPhysicalDeviceProperties physical_properties;
    vkGetPhysicalDeviceProperties(physical, &physical_properties);
    Log::debug("renderer", "Creating device from %s (%s)", physical_properties.deviceName,
               device_type(physical_properties.deviceType));

    std::uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &queue_family_count, nullptr);
    m_queue_families.resize(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &queue_family_count, m_queue_families.data());
    Log::trace("renderer", "Device has %d queue families", m_queue_families.size());
    for (const auto &queue_family : m_queue_families) {
        std::string flags;
        bool first = true;
        for (std::uint32_t i = 0; i < 4; i++) {
            if ((queue_family.queueFlags & (1U << i)) != 0) {
                if (!first) {
                    flags += '/';
                }
                first = false;
                flags += queue_flag(1U << i);
            }
        }
        Log::trace("renderer", " - %d queues capable of %s", queue_family.queueCount, flags.c_str());
    }

    Vector<VkDeviceQueueCreateInfo> queue_cis;
    const float queue_priority = 1.0F;
    for (std::uint32_t i = 0; i < m_queue_families.size(); i++) {
        VkDeviceQueueCreateInfo queue_ci{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = i,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        };
        queue_cis.push(queue_ci);
    }
    VkPhysicalDeviceFeatures device_features{
        .samplerAnisotropy = VK_TRUE,
    };
    VkDeviceCreateInfo device_ci{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = queue_cis.size(),
        .pQueueCreateInfos = queue_cis.data(),
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = &k_swapchain_extension_name,
        .pEnabledFeatures = &device_features,
    };
    ENSURE(vkCreateDevice(physical, &device_ci, nullptr, &m_device) == VK_SUCCESS);

    Log::trace("renderer", "Creating VMA allocator");
    VmaAllocatorCreateInfo allocator_ci{
        .physicalDevice = physical,
        .device = m_device,
        .instance = *instance,
    };
    ENSURE(vmaCreateAllocator(&allocator_ci, &m_allocator) == VK_SUCCESS);
}

Device::~Device() {
    vmaDestroyAllocator(m_allocator);
    vkDestroyDevice(m_device, nullptr);
}
