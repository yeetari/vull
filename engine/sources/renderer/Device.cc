#include <vull/renderer/Device.hh>

#include <vull/renderer/Buffer.hh>
#include <vull/renderer/Image.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Log.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Vector.hh>

#include <cstdint>
#include <new>
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

VkBufferUsageFlags buffer_usage(BufferType type) {
    switch (type) {
    case BufferType::IndexBuffer:
        return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    case BufferType::StorageBuffer:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    case BufferType::UniformBuffer:
        return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    case BufferType::VertexBuffer:
        return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    default:
        ENSURE_NOT_REACHED();
    }
}

VkMemoryPropertyFlags memory_flags(MemoryUsage usage) {
    switch (usage) {
    case MemoryUsage::CpuToGpu:
        return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    case MemoryUsage::GpuOnly:
        return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    default:
        ENSURE_NOT_REACHED();
    }
}

} // namespace

Device::Device(VkPhysicalDevice physical) : m_physical(physical) {
    VkPhysicalDeviceProperties physical_properties;
    vkGetPhysicalDeviceProperties(physical, &physical_properties);
    Log::debug("renderer", "Creating device from %s (%s)", static_cast<const char *>(physical_properties.deviceName),
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
            if ((queue_family.queueFlags & (1u << i)) != 0) {
                if (!first) {
                    flags += '/';
                }
                first = false;
                flags += queue_flag(1u << i);
            }
        }
        Log::trace("renderer", " - %d queues capable of %s", queue_family.queueCount, flags.c_str());
    }

    Vector<VkDeviceQueueCreateInfo> queue_cis;
    const float queue_priority = 1.0f;
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

    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(physical, &memory_properties);
    m_memory_types.ensure_capacity(memory_properties.memoryTypeCount);
    for (std::uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
        m_memory_types.push(memory_properties.memoryTypes[i]);
    }
}

Device::~Device() {
    vkDestroyDevice(m_device, nullptr);
}

Optional<std::uint32_t> Device::find_memory_type(const VkMemoryRequirements &requirements,
                                                 VkMemoryPropertyFlags flags) const {
    for (std::uint32_t i = 0; const auto &memory_type : m_memory_types) {
        if ((requirements.memoryTypeBits & (1u << i++)) == 0) {
            continue;
        }
        if ((memory_type.propertyFlags & flags) != flags) {
            continue;
        }
        return i - 1;
    }
    return {};
}

VkDeviceMemory Device::allocate_memory(const VkMemoryRequirements &requirements, MemoryUsage usage, bool dedicated,
                                       VkBuffer dedicated_buffer, VkImage dedicated_image) const {
    auto memory_type_index = find_memory_type(requirements, memory_flags(usage));
    ENSURE(memory_type_index);

    VkMemoryAllocateInfo memory_ai{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = *memory_type_index,
    };
    VkMemoryDedicatedAllocateInfo dedicated_ai{
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
        .image = dedicated_image,
        .buffer = dedicated_buffer,
    };
    if (dedicated) {
        memory_ai.pNext = &dedicated_ai;
    }
    VkDeviceMemory memory;
    ENSURE(vkAllocateMemory(m_device, &memory_ai, nullptr, &memory) == VK_SUCCESS);
    return memory;
}

Buffer Device::create_buffer(std::size_t size, BufferType type, MemoryUsage memory_usage, bool dedicated) const {
    VkBufferCreateInfo buffer_ci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = buffer_usage(type),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkBuffer buffer;
    ENSURE(vkCreateBuffer(m_device, &buffer_ci, nullptr, &buffer) == VK_SUCCESS);

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(m_device, buffer, &memory_requirements);

    VkDeviceMemory memory = allocate_memory(memory_requirements, memory_usage, dedicated, buffer, nullptr);
    ENSURE(vkBindBufferMemory(m_device, buffer, memory, 0) == VK_SUCCESS);
    return {this, buffer, memory};
}

Image Device::create_image(const VkImageCreateInfo &image_ci, MemoryUsage memory_usage, bool dedicated) const {
    VkImage image;
    ENSURE(vkCreateImage(m_device, &image_ci, nullptr, &image) == VK_SUCCESS);

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(m_device, image, &memory_requirements);

    VkDeviceMemory memory = allocate_memory(memory_requirements, memory_usage, dedicated, nullptr, image);
    ENSURE(vkBindImageMemory(m_device, image, memory, 0) == VK_SUCCESS);
    return {this, image, memory};
}
