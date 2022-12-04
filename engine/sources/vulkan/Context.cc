#include <vull/vulkan/Context.hh>

#include <vull/core/Log.hh>
#include <vull/maths/Common.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Enum.hh>
#include <vull/support/HashMap.hh>
#include <vull/support/HashSet.hh>
#include <vull/support/Optional.hh>
#include <vull/support/String.hh>
#include <vull/support/StringBuilder.hh>
#include <vull/support/StringView.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/Allocation.hh>
#include <vull/vulkan/Allocator.hh>
#include <vull/vulkan/Buffer.hh>
#include <vull/vulkan/ContextTable.hh>
#include <vull/vulkan/MemoryUsage.hh>
#include <vull/vulkan/Vulkan.hh>

#include <dlfcn.h>
#include <stdint.h>

namespace vull::vk {
namespace {

#define VK_MAKE_VERSION(major, minor, patch)                                                                           \
    ((static_cast<uint32_t>(major) << 22u) | (static_cast<uint32_t>(minor) << 12u) | static_cast<uint32_t>(patch))

const char *queue_flag_string(uint32_t bit) {
    switch (static_cast<vkb::QueueFlags>(bit)) {
    case vkb::QueueFlags::Graphics:
        return "G";
    case vkb::QueueFlags::Compute:
        return "C";
    case vkb::QueueFlags::Transfer:
        return "T";
    case vkb::QueueFlags::SparseBinding:
        return "SB";
    default:
        return "?";
    }
}

// TODO: This should be generated in Vulkan.hh
vkb::MemoryPropertyFlags operator~(vkb::MemoryPropertyFlags flags) {
    return static_cast<vkb::MemoryPropertyFlags>(~static_cast<uint32_t>(flags));
}

} // namespace

Context::Context() : ContextTable{} {
    void *libvulkan = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (libvulkan == nullptr) {
        libvulkan = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
        VULL_ENSURE(libvulkan != nullptr, "Failed to find vulkan");
    }
    auto *vkGetInstanceProcAddr =
        reinterpret_cast<vkb::PFN_vkGetInstanceProcAddr>(dlsym(libvulkan, "vkGetInstanceProcAddr"));
    load_loader(vkGetInstanceProcAddr);

    Array enabled_instance_extensions{
        "VK_KHR_get_physical_device_properties2",
        "VK_KHR_surface",
        "VK_KHR_xcb_surface",
    };
    vkb::ApplicationInfo application_info{
        .sType = vkb::StructureType::ApplicationInfo,
        .apiVersion = VK_MAKE_VERSION(1, 3, 0),
    };
    vkb::InstanceCreateInfo instance_ci{
        .sType = vkb::StructureType::InstanceCreateInfo,
        .pApplicationInfo = &application_info,
        .enabledExtensionCount = enabled_instance_extensions.size(),
        .ppEnabledExtensionNames = enabled_instance_extensions.data(),
    };
#ifndef NDEBUG
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    Vector<vkb::LayerProperties> layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, layers.data());

    const char *validation_layer_name = "VK_LAYER_KHRONOS_validation";
    bool has_validation_layer = false;
    for (const auto &layer : layers) {
        if (vull::StringView(static_cast<const char *>(layer.layerName)) == validation_layer_name) {
            has_validation_layer = true;
            break;
        }
    }
    if (has_validation_layer) {
        instance_ci.enabledLayerCount = 1;
        instance_ci.ppEnabledLayerNames = &validation_layer_name;
    } else {
        vull::warn("[vulkan] Validation layer not present");
    }
#endif
    VULL_ENSURE(vkCreateInstance(&instance_ci, &m_instance) == vkb::Result::Success);
    load_instance(vkGetInstanceProcAddr);

    // TODO: Better device selection.
    uint32_t physical_device_count = 1;
    vkb::Result enumeration_result = vkEnumeratePhysicalDevices(&physical_device_count, &m_physical_device);
    VULL_ENSURE(enumeration_result == vkb::Result::Success || enumeration_result == vkb::Result::Incomplete);
    VULL_ENSURE(physical_device_count == 1);

    vkGetPhysicalDeviceProperties(&m_properties);
    vull::info("[vulkan] Creating device from {}", m_properties.deviceName);

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(&queue_family_count, nullptr);
    m_queue_families.ensure_size(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(&queue_family_count, m_queue_families.data());
    vull::debug("[vulkan] Device has {} queue families", m_queue_families.size());

    Vector<vkb::DeviceQueueCreateInfo> queue_cis;
    const float queue_priority = 1.0f;
    for (uint32_t i = 0; i < m_queue_families.size(); i++) {
        queue_cis.push({
            .sType = vkb::StructureType::DeviceQueueCreateInfo,
            .queueFamilyIndex = i,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        });

        StringBuilder flags_string;
        bool first = true;
        for (uint32_t bit = 0; bit < 32; bit++) {
            if ((static_cast<uint32_t>(m_queue_families[i].queueFlags) & (1u << bit)) == 0u) {
                continue;
            }
            if (!vull::exchange(first, false)) {
                flags_string.append('/');
            }
            flags_string.append("{}", queue_flag_string(1u << bit));
        }
        vull::debug("[vulkan]  - {} queues capable of {}", m_queue_families[i].queueCount, flags_string.build());
    }

    vkb::PhysicalDeviceFeatures2 device_10_features{
        .sType = vkb::StructureType::PhysicalDeviceFeatures2,
        .features{
            .samplerAnisotropy = true,
        },
    };
    vkb::PhysicalDeviceVulkan12Features device_12_features{
        .sType = vkb::StructureType::PhysicalDeviceVulkan12Features,
        .pNext = &device_10_features,
        .shaderSampledImageArrayNonUniformIndexing = true,
        .descriptorBindingPartiallyBound = true,
        .runtimeDescriptorArray = true,
        .scalarBlockLayout = true,
        .timelineSemaphore = true,
        // Extension promoted to vk 1.2, feature must be supported in vk 1.3
        .bufferDeviceAddress = true,
    };
    vkb::PhysicalDeviceVulkan13Features device_13_features{
        .sType = vkb::StructureType::PhysicalDeviceVulkan13Features,
        .pNext = &device_12_features,
        .synchronization2 = true,
        .dynamicRendering = true,
    };
    vkb::PhysicalDeviceShaderAtomicFloat2FeaturesEXT atomic_float_min_max_features{
        .sType = vkb::StructureType::PhysicalDeviceShaderAtomicFloat2FeaturesEXT,
        .pNext = &device_13_features,
        .shaderSharedFloat32AtomicMinMax = true,
    };

    Array enabled_device_extensions{
        "VK_EXT_shader_atomic_float",
        "VK_EXT_shader_atomic_float2",
        "VK_KHR_swapchain",
    };
    vkb::DeviceCreateInfo device_ci{
        .sType = vkb::StructureType::DeviceCreateInfo,
        .pNext = &atomic_float_min_max_features,
        .queueCreateInfoCount = queue_cis.size(),
        .pQueueCreateInfos = queue_cis.data(),
        .enabledExtensionCount = enabled_device_extensions.size(),
        .ppEnabledExtensionNames = enabled_device_extensions.data(),
    };
    VULL_ENSURE(vkCreateDevice(&device_ci, &m_device) == vkb::Result::Success);
    load_device();

    vkGetPhysicalDeviceMemoryProperties(&m_memory_properties);
    for (uint32_t i = 0; i < m_memory_properties.memoryTypeCount; i++) {
        m_allocators.emplace(new Allocator(*this, i));
    }

    vull::debug("[vulkan] Memory usage -> memory type mapping:");
    auto get_dummy_buffer_requirements = [this](vkb::BufferUsage usage) {
        vkb::BufferCreateInfo create_info{
            .sType = vkb::StructureType::BufferCreateInfo,
            .size = 65536,
            .usage = usage,
        };
        vkb::DeviceBufferMemoryRequirements requirements_info{
            .sType = vkb::StructureType::DeviceBufferMemoryRequirements,
            .pCreateInfo = &create_info,
        };
        vkb::MemoryRequirements2 requirements{
            .sType = vkb::StructureType::MemoryRequirements2,
        };
        vkGetDeviceBufferMemoryRequirements(&requirements_info, &requirements);
        return requirements.memoryRequirements;
    };

    const auto descriptor_requirements = get_dummy_buffer_requirements(vkb::BufferUsage::ResourceDescriptorBufferEXT);
    const auto indirect_requirements = get_dummy_buffer_requirements(vkb::BufferUsage::IndirectBuffer);
    const auto ssbo_requirements = get_dummy_buffer_requirements(vkb::BufferUsage::StorageBuffer);
    const auto ubo_requirements = get_dummy_buffer_requirements(vkb::BufferUsage::UniformBuffer);
    auto print_for = [&](MemoryUsage usage) {
        HashMap<uint32_t, Vector<String>> map;
        map[allocator_for(descriptor_requirements, usage).memory_type_index()].push("descriptor");
        map[allocator_for(indirect_requirements, usage).memory_type_index()].push("indirect");
        map[allocator_for(ssbo_requirements, usage).memory_type_index()].push("ssbo");
        map[allocator_for(ubo_requirements, usage).memory_type_index()].push("ubo");

        vull::debug("[vulkan]  - {}", vull::enum_name(usage));
        for (const auto &[index, types] : map) {
            StringBuilder sb;
            sb.append("[vulkan]   - {} (", index);
            for (bool first = true; const auto &type : types) {
                if (!vull::exchange(first, false)) {
                    sb.append(", ");
                }
                sb.append(type);
            }
            sb.append(")");
            vull::debug(sb.build());
        }
    };
    print_for(MemoryUsage::DeviceOnly);
    print_for(MemoryUsage::HostOnly);
    print_for(MemoryUsage::HostToDevice);
    print_for(MemoryUsage::DeviceToHost);
}

Context::~Context() {
    m_allocators.clear();
    vkDestroyDevice();
    vkDestroyInstance();
}

Allocator &Context::allocator_for(const vkb::MemoryRequirements &requirements, MemoryUsage usage) {
    auto required_flags = vkb::MemoryPropertyFlags::None;
    auto desirable_flags = vkb::MemoryPropertyFlags::None;
    switch (usage) {
    case MemoryUsage::DeviceOnly:
        required_flags |= vkb::MemoryPropertyFlags::DeviceLocal;
        break;
    case MemoryUsage::DeviceToHost:
        required_flags |= vkb::MemoryPropertyFlags::HostVisible;
        desirable_flags |= vkb::MemoryPropertyFlags::HostCached;
        break;
    case MemoryUsage::HostToDevice:
        desirable_flags |= vkb::MemoryPropertyFlags::DeviceLocal;
        [[fallthrough]];
    case MemoryUsage::HostOnly:
        required_flags |= vkb::MemoryPropertyFlags::HostVisible | vkb::MemoryPropertyFlags::HostCoherent;
        break;
    }

    Optional<Allocator &> best_allocator;
    uint32_t best_cost = UINT32_MAX;
    for (uint32_t index = 0; index < m_memory_properties.memoryTypeCount; index++) {
        if ((requirements.memoryTypeBits & (1u << index)) == 0u) {
            // Memory type not acceptable for this allocation.
            continue;
        }

        const auto flags = m_memory_properties.memoryTypes[index].propertyFlags;
        if ((flags & required_flags) != required_flags) {
            // Memory type doesn't have all the required flags.
            continue;
        }

        const auto cost = static_cast<uint32_t>(vull::popcount(vull::to_underlying(desirable_flags & ~flags)));
        if (cost < best_cost) {
            best_cost = cost;
            best_allocator = *m_allocators[index];
        }
        if (cost == 0) {
            // Perfect match.
            break;
        }
    }
    VULL_ENSURE(best_allocator);
    return *best_allocator;
}

Allocation Context::allocate_memory(const vkb::MemoryRequirements &requirements, vk::MemoryUsage usage) {
    return allocator_for(requirements, usage).allocate(requirements);
}

Allocation Context::bind_memory(vkb::Buffer buffer, vk::MemoryUsage usage) {
    vkb::MemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(buffer, &requirements);

    auto allocation = allocate_memory(requirements, usage);
    const auto &info = allocation.info();
    VULL_ENSURE(vkBindBufferMemory(buffer, info.memory, info.offset) == vkb::Result::Success);
    return allocation;
}

Allocation Context::bind_memory(vkb::Image image, vk::MemoryUsage usage) {
    vkb::MemoryRequirements requirements{};
    vkGetImageMemoryRequirements(image, &requirements);

    auto allocation = allocate_memory(requirements, usage);
    const auto &info = allocation.info();
    VULL_ENSURE(vkBindImageMemory(image, info.memory, info.offset) == vkb::Result::Success);
    return allocation;
}

Buffer Context::create_buffer(vkb::DeviceSize size, vkb::BufferUsage usage, MemoryUsage memory_usage) {
    vkb::BufferCreateInfo buffer_ci{
        .sType = vkb::StructureType::BufferCreateInfo,
        .size = size,
        .usage = usage,
        .sharingMode = vkb::SharingMode::Exclusive,
    };
    vkb::Buffer buffer;
    VULL_ENSURE(vkCreateBuffer(&buffer_ci, &buffer) == vkb::Result::Success);

    vkb::MemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(buffer, &requirements);

    auto allocation = allocate_memory(requirements, memory_usage);
    const auto &info = allocation.info();
    VULL_ENSURE(vkBindBufferMemory(buffer, info.memory, info.offset) == vkb::Result::Success);
    return {vull::move(allocation), buffer, usage};
}

float Context::timestamp_elapsed(uint64_t start, uint64_t end) const {
    return (static_cast<float>(end - start) * m_properties.limits.timestampPeriod) / 1000000000.0f;
}

} // namespace vull::vk
