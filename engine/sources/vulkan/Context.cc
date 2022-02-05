#include <vull/vulkan/Context.hh>

#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Lsan.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/ContextTable.hh>

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace vull {

Context::Context() : ContextTable{} {
    LsanDisabler lsan_disabler;
    void *libvulkan = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (libvulkan == nullptr) {
        libvulkan = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
        VULL_ENSURE(libvulkan != nullptr, "Failed to find vulkan");
    }
    auto *vkGetInstanceProcAddr =
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(libvulkan, "vkGetInstanceProcAddr"));
    load_loader(vkGetInstanceProcAddr);

    Array enabled_instance_extensions{
        "VK_KHR_get_physical_device_properties2",
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
        .enabledExtensionCount = enabled_instance_extensions.size(),
        .ppEnabledExtensionNames = enabled_instance_extensions.data(),
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
    VULL_ENSURE(vkCreateInstance(&instance_ci, &m_instance) == VK_SUCCESS);
    load_instance(vkGetInstanceProcAddr);

    uint32_t physical_device_count = 1;
    VkResult enumeration_result = vkEnumeratePhysicalDevices(&physical_device_count, &m_physical_device);
    VULL_ENSURE(enumeration_result == VK_SUCCESS || enumeration_result == VK_INCOMPLETE);
    VULL_ENSURE(physical_device_count == 1);

    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(&memory_properties);
    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
        m_memory_types.push(memory_properties.memoryTypes[i]);
    }

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(&queue_family_count, nullptr);
    m_queue_families.ensure_size(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(&queue_family_count, m_queue_families.data());

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

    VkPhysicalDeviceFeatures device_features{
        .tessellationShader = VK_TRUE,
        .fillModeNonSolid = VK_TRUE,
    };
    VkPhysicalDeviceVulkan12Features device_12_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    };
    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
        .pNext = &device_12_features,
        .dynamicRendering = VK_TRUE,
    };
    VkPhysicalDeviceShaderAtomicFloat2FeaturesEXT atomic_float_min_max_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_2_FEATURES_EXT,
        .pNext = &dynamic_rendering_features,
        .shaderSharedFloat32AtomicMinMax = VK_TRUE,
    };

    Array enabled_device_extensions{
        VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME,
        VK_EXT_SHADER_ATOMIC_FLOAT_2_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    VkDeviceCreateInfo device_ci{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &atomic_float_min_max_features,
        .queueCreateInfoCount = queue_cis.size(),
        .pQueueCreateInfos = queue_cis.data(),
        .enabledExtensionCount = enabled_device_extensions.size(),
        .ppEnabledExtensionNames = enabled_device_extensions.data(),
        .pEnabledFeatures = &device_features,
    };
    VULL_ENSURE(vkCreateDevice(&device_ci, &m_device) == VK_SUCCESS);
    load_device();
}

Context::~Context() {
    vkDestroyDevice();
    vkDestroyInstance();
}

} // namespace vull
