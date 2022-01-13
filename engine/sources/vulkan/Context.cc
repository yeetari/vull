#include <vull/vulkan/Context.hh>

#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Vector.hh>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace vull {

Context::Context() {
    constexpr Array enabled_extensions{
        "VK_KHR_surface",
        "VK_KHR_xcb_surface",
    };
    VkApplicationInfo application_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = VK_API_VERSION_1_2,
    };
    VkInstanceCreateInfo instance_ci{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &application_info,
        .enabledExtensionCount = enabled_extensions.size(),
        .ppEnabledExtensionNames = enabled_extensions.data(),
    };
#ifndef NDEBUG
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    Vector<VkLayerProperties> layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, layers.data());

    const char *validation_layer_name = "VK_LAYER_KHRONOS_validation";
    bool has_validation_layer = false;
    for (const auto &layer : layers) {
        const auto *layer_name = static_cast<const char *>(layer.layerName);
        if (strncmp(layer_name, validation_layer_name, strlen(validation_layer_name)) == 0) {
            has_validation_layer = true;
            break;
        }
    }
    if (has_validation_layer) {
        instance_ci.enabledLayerCount = 1;
        instance_ci.ppEnabledLayerNames = &validation_layer_name;
    } else {
        fputs("Validation layer not present!", stderr);
    }
#endif
    VULL_ENSURE(vkCreateInstance(&instance_ci, nullptr, &m_instance) == VK_SUCCESS);

    uint32_t physical_device_count = 1;
    VkResult enumeration_result = vkEnumeratePhysicalDevices(m_instance, &physical_device_count, &m_physical_device);
    VULL_ENSURE(enumeration_result == VK_SUCCESS || enumeration_result == VK_INCOMPLETE);
    VULL_ENSURE(physical_device_count == 1);

    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(m_physical_device, &memory_properties);
    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
        m_memory_types.push(memory_properties.memoryTypes[i]);
    }

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, nullptr);
    m_queue_families.ensure_size(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, m_queue_families.data());

    Vector<VkDeviceQueueCreateInfo> queue_cis;
    const float queue_priority = 1.0f;
    for (uint32_t i = 0; i < m_queue_families.size(); i++) {
        queue_cis.push({
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = i,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        });
    }

    const char *swapchain_extension_name = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    VkDeviceCreateInfo device_ci{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = queue_cis.size(),
        .pQueueCreateInfos = queue_cis.data(),
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = &swapchain_extension_name,
    };
    VULL_ENSURE(vkCreateDevice(m_physical_device, &device_ci, nullptr, &m_device) == VK_SUCCESS);
}

Context::~Context() {
    vkDestroyDevice(m_device, nullptr);
    vkDestroyInstance(m_instance, nullptr);
}

} // namespace vull
