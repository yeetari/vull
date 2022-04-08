#include <vull/vulkan/Context.hh>

#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Lsan.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/ContextTable.hh>
#include <vull/vulkan/Vulkan.hh>

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace vull {
namespace {

#define VK_MAKE_VERSION(major, minor, patch)                                                                           \
    ((static_cast<uint32_t>(major) << 22u) | (static_cast<uint32_t>(minor) << 12u) | static_cast<uint32_t>(patch))

vk::MemoryPropertyFlags memory_flags(MemoryType type) {
    switch (type) {
    case MemoryType::DeviceLocal:
        return vk::MemoryPropertyFlags::DeviceLocal;
    case MemoryType::HostVisible:
        return vk::MemoryPropertyFlags::DeviceLocal | vk::MemoryPropertyFlags::HostVisible |
               vk::MemoryPropertyFlags::HostCoherent;
    case MemoryType::Staging:
        return vk::MemoryPropertyFlags::HostVisible | vk::MemoryPropertyFlags::HostCoherent;
    }
    VULL_ENSURE_NOT_REACHED();
}

} // namespace

VkContext::VkContext() : ContextTable{} {
    LsanDisabler lsan_disabler;
    void *libvulkan = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (libvulkan == nullptr) {
        libvulkan = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
        VULL_ENSURE(libvulkan != nullptr, "Failed to find vulkan");
    }
    auto *vkGetInstanceProcAddr =
        reinterpret_cast<vk::PFN_vkGetInstanceProcAddr>(dlsym(libvulkan, "vkGetInstanceProcAddr"));
    load_loader(vkGetInstanceProcAddr);

    Array enabled_instance_extensions{
        "VK_KHR_get_physical_device_properties2",
        "VK_KHR_surface",
        "VK_KHR_xcb_surface",
    };
    vk::ApplicationInfo application_info{
        .sType = vk::StructureType::ApplicationInfo,
        .apiVersion = VK_MAKE_VERSION(1, 3, 0),
    };
    vk::InstanceCreateInfo instance_ci{
        .sType = vk::StructureType::InstanceCreateInfo,
        .pApplicationInfo = &application_info,
        .enabledExtensionCount = enabled_instance_extensions.size(),
        .ppEnabledExtensionNames = enabled_instance_extensions.data(),
    };
#ifndef NDEBUG
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    Vector<vk::LayerProperties> layers(layer_count);
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
    VULL_ENSURE(vkCreateInstance(&instance_ci, &m_instance) == vk::Result::Success);
    load_instance(vkGetInstanceProcAddr);

    uint32_t physical_device_count = 1;
    vk::Result enumeration_result = vkEnumeratePhysicalDevices(&physical_device_count, &m_physical_device);
    VULL_ENSURE(enumeration_result == vk::Result::Success || enumeration_result == vk::Result::Incomplete);
    VULL_ENSURE(physical_device_count == 1);

    vk::PhysicalDeviceMemoryProperties memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(&memory_properties);
    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
        m_memory_types.push(memory_properties.memoryTypes[i]);
    }

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(&queue_family_count, nullptr);
    m_queue_families.ensure_size(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(&queue_family_count, m_queue_families.data());

    Vector<vk::DeviceQueueCreateInfo> queue_cis;
    const float queue_priority = 1.0f;
    for (uint32_t i = 0; i < m_queue_families.size(); i++) {
        queue_cis.push({
            .sType = vk::StructureType::DeviceQueueCreateInfo,
            .queueFamilyIndex = i,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        });
    }

    vk::PhysicalDeviceFeatures device_features{
        .fillModeNonSolid = true,
    };
    vk::PhysicalDeviceVulkan12Features device_12_features{
        .sType = vk::StructureType::PhysicalDeviceVulkan12Features,
        .shaderSampledImageArrayNonUniformIndexing = true,
        .descriptorBindingPartiallyBound = true,
        .runtimeDescriptorArray = true,
        .scalarBlockLayout = true,
    };
    vk::PhysicalDeviceVulkan13Features device_13_features{
        .sType = vk::StructureType::PhysicalDeviceVulkan13Features,
        .pNext = &device_12_features,
        .dynamicRendering = true,
    };
    vk::PhysicalDeviceShaderAtomicFloat2FeaturesEXT atomic_float_min_max_features{
        .sType = vk::StructureType::PhysicalDeviceShaderAtomicFloat2FeaturesEXT,
        .pNext = &device_13_features,
        .shaderSharedFloat32AtomicMinMax = true,
    };

    Array enabled_device_extensions{
        "VK_EXT_shader_atomic_float",
        "VK_EXT_shader_atomic_float2",
        "VK_KHR_swapchain",
    };
    vk::DeviceCreateInfo device_ci{
        .sType = vk::StructureType::DeviceCreateInfo,
        .pNext = &atomic_float_min_max_features,
        .queueCreateInfoCount = queue_cis.size(),
        .pQueueCreateInfos = queue_cis.data(),
        .enabledExtensionCount = enabled_device_extensions.size(),
        .ppEnabledExtensionNames = enabled_device_extensions.data(),
        .pEnabledFeatures = &device_features,
    };
    VULL_ENSURE(vkCreateDevice(&device_ci, &m_device) == vk::Result::Success);
    load_device();
}

VkContext::~VkContext() {
    vkDestroyDevice();
    vkDestroyInstance();
}

uint32_t VkContext::find_memory_type_index(const vk::MemoryRequirements &requirements, MemoryType type) const {
    const auto flags = memory_flags(type);
    for (uint32_t i = 0; i < m_memory_types.size(); i++) {
        if ((requirements.memoryTypeBits & (1u << i)) == 0u) {
            continue;
        }
        if ((m_memory_types[i].propertyFlags & flags) != flags) {
            continue;
        }
        return i;
    }
    VULL_ENSURE_NOT_REACHED();
}

vk::DeviceMemory VkContext::allocate_memory(const vk::MemoryRequirements &requirements, MemoryType type) const {
    vk::MemoryAllocateInfo memory_ai{
        .sType = vk::StructureType::MemoryAllocateInfo,
        .allocationSize = requirements.size,
        .memoryTypeIndex = find_memory_type_index(requirements, type),
    };
    vk::DeviceMemory memory;
    VULL_ENSURE(vkAllocateMemory(&memory_ai, &memory) == vk::Result::Success);
    return memory;
}

} // namespace vull
