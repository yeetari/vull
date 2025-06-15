#include <vull/vulkan/context.hh>

#include <vull/container/array.hh>
#include <vull/container/hash_map.hh>
#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/maths/common.hh>
#include <vull/maths/random.hh>
#include <vull/support/assert.hh>
#include <vull/support/enum.hh>
#include <vull/support/optional.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/mutex.hh>
#include <vull/vulkan/allocation.hh>
#include <vull/vulkan/allocator.hh>
#include <vull/vulkan/buffer.hh>
#include <vull/vulkan/context_table.hh>
#include <vull/vulkan/fence.hh>
#include <vull/vulkan/image.hh>
#include <vull/vulkan/memory_usage.hh>
#include <vull/vulkan/queue.hh>
#include <vull/vulkan/sampler.hh>
#include <vull/vulkan/semaphore.hh>
#include <vull/vulkan/vulkan.hh>

#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

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

vkb::Bool validation_callback(vkb::DebugUtilsMessageSeverityFlagsEXT severity, vkb::DebugUtilsMessageTypeFlagsEXT,
                              const vkb::DebugUtilsMessengerCallbackDataEXT *callback_data, void *) {
    StringBuilder sb;
    sb.append(callback_data->pMessageIdName);
    sb.append('\n');
    for (uint32_t i = 0; i < callback_data->objectCount; i++) {
        const auto &object = callback_data->pObjects[i];
        sb.append("\t\t\t   [{}] {h}, type: {}", i, object.objectHandle, vull::to_underlying(object.objectType));
        if (object.pObjectName != nullptr) {
            sb.append(", name: {}", object.pObjectName);
        }
        if (i != callback_data->objectCount - 1) {
            sb.append('\n');
        }
    }

    if (severity == vkb::DebugUtilsMessageSeverityFlagsEXT::Warning) {
        vull::warn("[vulkan] Validation warning: {}", sb.build());
        return false;
    }

    vull::error("[vulkan] Validation error: {}", sb.build());
    vull::close_log();
    abort();
}

} // namespace

template <vkb::ObjectType ObjectType>
void Context::set_object_name(const void *object, StringView name) const {
    vkb::DebugUtilsObjectNameInfoEXT name_info{
        .sType = vkb::StructureType::DebugUtilsObjectNameInfoEXT,
        .objectType = ObjectType,
        .objectHandle = __builtin_bit_cast(uint64_t, object),
        .pObjectName = name.data(),
    };
    vkSetDebugUtilsObjectNameEXT(&name_info);
}

template <>
void Context::set_object_name(const vkb::DeviceMemory &object, StringView name) const {
    set_object_name<vkb::ObjectType::DeviceMemory>(object, name);
}
template <>
void Context::set_object_name(const Fence &object, StringView name) const {
    set_object_name<vkb::ObjectType::Fence>(*object, name);
}
template <>
void Context::set_object_name(const vkb::Sampler &object, StringView name) const {
    set_object_name<vkb::ObjectType::Sampler>(object, name);
}
template <>
void Context::set_object_name(const Semaphore &object, StringView name) const {
    set_object_name<vkb::ObjectType::Semaphore>(*object, name);
}

Context::Context(bool enable_validation) : ContextTable{} {
    void *libvulkan = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (libvulkan == nullptr) {
        libvulkan = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
        VULL_ENSURE(libvulkan != nullptr, "Failed to find vulkan");
    }
    auto *vkGetInstanceProcAddr =
        reinterpret_cast<vkb::PFN_vkGetInstanceProcAddr>(dlsym(libvulkan, "vkGetInstanceProcAddr"));
    load_loader(vkGetInstanceProcAddr);

    Array enabled_instance_extensions{
        "VK_EXT_debug_utils",
        "VK_KHR_get_physical_device_properties2",
        "VK_KHR_surface",
        "VK_KHR_xcb_surface",
    };
    vkb::ApplicationInfo application_info{
        .sType = vkb::StructureType::ApplicationInfo,
        .apiVersion = VK_MAKE_VERSION(1, 3, 0),
    };
    const auto enable_sync_validation = vkb::ValidationFeatureEnableEXT::SynchronizationValidation;
    vkb::ValidationFeaturesEXT validation_features{
        .sType = vkb::StructureType::ValidationFeaturesEXT,
        .enabledValidationFeatureCount = 1,
        .pEnabledValidationFeatures = &enable_sync_validation,
    };
    vkb::InstanceCreateInfo instance_ci{
        .sType = vkb::StructureType::InstanceCreateInfo,
        .pNext = &validation_features,
        .pApplicationInfo = &application_info,
        .enabledExtensionCount = enabled_instance_extensions.size(),
        .ppEnabledExtensionNames = enabled_instance_extensions.data(),
    };
    const char *validation_layer_name = "VK_LAYER_KHRONOS_validation";
    if (enable_validation) {
        uint32_t layer_count = 0;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
        Vector<vkb::LayerProperties> layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, layers.data());

        bool has_validation_layer = false;
        for (const auto &layer : layers) {
            if (vull::StringView(static_cast<const char *>(layer.layerName)) == validation_layer_name) {
                has_validation_layer = true;
                break;
            }
        }
        if (has_validation_layer) {
            vull::info("[vulkan] Enabling validation layer");
            instance_ci.enabledLayerCount = 1;
            instance_ci.ppEnabledLayerNames = &validation_layer_name;
        } else {
            vull::warn("[vulkan] Validation layer not present");
        }
    }
    VULL_ENSURE(vkCreateInstance(&instance_ci, &m_instance) == vkb::Result::Success);
    load_instance(vkGetInstanceProcAddr);

    vkb::DebugUtilsMessengerCreateInfoEXT debug_utils_messenger_ci{
        .sType = vkb::StructureType::DebugUtilsMessengerCreateInfoEXT,
        .messageSeverity =
            vkb::DebugUtilsMessageSeverityFlagsEXT::Warning | vkb::DebugUtilsMessageSeverityFlagsEXT::Error,
        .messageType = vkb::DebugUtilsMessageTypeFlagsEXT::Validation,
        .pfnUserCallback = &validation_callback,
    };
    vkCreateDebugUtilsMessengerEXT(&debug_utils_messenger_ci, &m_debug_utils_messenger);

    // TODO: Better device selection.
    uint32_t physical_device_count = 1;
    vkb::Result enumeration_result = vkEnumeratePhysicalDevices(&physical_device_count, &m_physical_device);
    VULL_ENSURE(enumeration_result == vkb::Result::Success || enumeration_result == vkb::Result::Incomplete);
    VULL_ENSURE(physical_device_count == 1);

    m_descriptor_buffer_properties.sType = vkb::StructureType::PhysicalDeviceDescriptorBufferPropertiesEXT;
    vkb::PhysicalDeviceProperties2 properties{
        .sType = vkb::StructureType::PhysicalDeviceProperties2,
        .pNext = &m_descriptor_buffer_properties,
    };
    vkGetPhysicalDeviceProperties2(&properties);
    m_properties = properties.properties;
    vull::info("[vulkan] Creating device from {}", m_properties.deviceName);

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(&queue_family_count, nullptr);
    Vector<vkb::QueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(&queue_family_count, queue_families.data());
    vull::debug("[vulkan] Device has {} queue families", queue_families.size());

    Vector<vkb::DeviceQueueCreateInfo> queue_cis;
    const float queue_priority = 1.0f;
    for (uint32_t i = 0; i < queue_families.size(); i++) {
        queue_cis.push({
            .sType = vkb::StructureType::DeviceQueueCreateInfo,
            .queueFamilyIndex = i,
            .queueCount = queue_families[i].queueCount,
            .pQueuePriorities = &queue_priority,
        });

        StringBuilder flags_string;
        bool first = true;
        for (uint32_t bit = 0; bit < 32; bit++) {
            if ((static_cast<uint32_t>(queue_families[i].queueFlags) & (1u << bit)) == 0u) {
                continue;
            }
            if (!vull::exchange(first, false)) {
                flags_string.append('/');
            }
            flags_string.append("{}", queue_flag_string(1u << bit));
        }
        vull::debug("[vulkan]  - {} queues capable of {}", queue_families[i].queueCount, flags_string.build());
    }

    // TODO: Some features (like pipelineStatisticsQuery) should be optional.
    vkb::PhysicalDeviceFeatures2 device_10_features{
        .sType = vkb::StructureType::PhysicalDeviceFeatures2,
        .features{
            .multiDrawIndirect = true,
            .samplerAnisotropy = true,
            .pipelineStatisticsQuery = true,
            .shaderInt16 = true,
        },
    };
    vkb::PhysicalDeviceVulkan11Features device_11_features{
        .sType = vkb::StructureType::PhysicalDeviceVulkan11Features,
        .pNext = &device_10_features,
        .storageBuffer16BitAccess = true,
        .shaderDrawParameters = true,
    };
    vkb::PhysicalDeviceVulkan12Features device_12_features{
        .sType = vkb::StructureType::PhysicalDeviceVulkan12Features,
        .pNext = &device_11_features,
        .drawIndirectCount = true,
        .shaderSampledImageArrayNonUniformIndexing = true,
        .descriptorBindingPartiallyBound = true,
        .descriptorBindingVariableDescriptorCount = true,
        .runtimeDescriptorArray = true,
        .samplerFilterMinmax = true,
        .scalarBlockLayout = true,
        .hostQueryReset = true,
        .timelineSemaphore = true,
        // Extension promoted to vk 1.2, feature must be supported in vk 1.3
        .bufferDeviceAddress = true,
    };
    vkb::PhysicalDeviceVulkan13Features device_13_features{
        .sType = vkb::StructureType::PhysicalDeviceVulkan13Features,
        .pNext = &device_12_features,
        .synchronization2 = true,
        .shaderZeroInitializeWorkgroupMemory = true,
        .dynamicRendering = true,
        .maintenance4 = true,
    };
    vkb::PhysicalDeviceShaderAtomicFloat2FeaturesEXT atomic_float_min_max_features{
        .sType = vkb::StructureType::PhysicalDeviceShaderAtomicFloat2FeaturesEXT,
        .pNext = &device_13_features,
        .shaderSharedFloat32AtomicMinMax = true,
    };
    vkb::PhysicalDeviceDescriptorBufferFeaturesEXT descriptor_buffer_features{
        .sType = vkb::StructureType::PhysicalDeviceDescriptorBufferFeaturesEXT,
        .pNext = &atomic_float_min_max_features,
        .descriptorBuffer = true,
    };

    Array enabled_device_extensions{
        "VK_EXT_descriptor_buffer",
        "VK_EXT_shader_atomic_float",
        "VK_EXT_shader_atomic_float2",
        "VK_KHR_swapchain",
    };
    vkb::DeviceCreateInfo device_ci{
        .sType = vkb::StructureType::DeviceCreateInfo,
        .pNext = &descriptor_buffer_features,
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

    auto create_queues = [&](uint32_t family_index, Vector<Queue &> &vector) {
        for (uint32_t index = 0; index < queue_families[family_index].queueCount; index++) {
            auto &queue = *m_queues.emplace(new Queue(*this, family_index, index));
            vector.push(queue);
        }
    };

    for (uint32_t family_index = 0; family_index < queue_families.size(); family_index++) {
        auto flags = queue_families[family_index].queueFlags;
        if ((flags & vkb::QueueFlags::Graphics) != vkb::QueueFlags::None) {
            create_queues(family_index, m_graphics_queues);
        } else if ((flags & vkb::QueueFlags::Compute) != vkb::QueueFlags::None) {
            create_queues(family_index, m_compute_queues);
        } else if ((flags & vkb::QueueFlags::Transfer) != vkb::QueueFlags::None) {
            create_queues(family_index, m_transfer_queues);
        }
    }

    VULL_ENSURE(!m_graphics_queues.empty());
    if (m_compute_queues.empty()) {
        m_compute_queues.extend(m_graphics_queues);
    }
    if (m_transfer_queues.empty()) {
        m_transfer_queues.extend(m_compute_queues);
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

    vkb::SamplerCreateInfo nearest_sampler_ci{
        .sType = vkb::StructureType::SamplerCreateInfo,
        .magFilter = vkb::Filter::Nearest,
        .minFilter = vkb::Filter::Nearest,
        .mipmapMode = vkb::SamplerMipmapMode::Nearest,
        .maxLod = vkb::k_lod_clamp_none,
    };
    VULL_ENSURE(vkCreateSampler(&nearest_sampler_ci, &m_nearest_sampler) == vkb::Result::Success);
    set_object_name(m_nearest_sampler, "Nearest sampler");

    vkb::SamplerCreateInfo linear_sampler_ci{
        .sType = vkb::StructureType::SamplerCreateInfo,
        .magFilter = vkb::Filter::Linear,
        .minFilter = vkb::Filter::Linear,
        .mipmapMode = vkb::SamplerMipmapMode::Linear,
        .anisotropyEnable = true,
        .maxAnisotropy = 16.0f,
        .maxLod = vkb::k_lod_clamp_none,
    };
    VULL_ENSURE(vkCreateSampler(&linear_sampler_ci, &m_linear_sampler) == vkb::Result::Success);
    set_object_name(m_linear_sampler, "Linear sampler");

    vkb::SamplerReductionModeCreateInfo depth_reduction_mode_ci{
        .sType = vkb::StructureType::SamplerReductionModeCreateInfo,
        .reductionMode = vkb::SamplerReductionMode::Min,
    };
    vkb::SamplerCreateInfo depth_reduce_sampler_ci{
        .sType = vkb::StructureType::SamplerCreateInfo,
        .pNext = &depth_reduction_mode_ci,
        .magFilter = vkb::Filter::Linear,
        .minFilter = vkb::Filter::Linear,
        .mipmapMode = vkb::SamplerMipmapMode::Nearest,
        .maxLod = vkb::k_lod_clamp_none,
    };
    VULL_ENSURE(vkCreateSampler(&depth_reduce_sampler_ci, &m_depth_reduce_sampler) == vkb::Result::Success);
    set_object_name(m_depth_reduce_sampler, "Depth reduce sampler");

    vkb::SamplerCreateInfo shadow_sampler_ci{
        .sType = vkb::StructureType::SamplerCreateInfo,
        .magFilter = vkb::Filter::Linear,
        .minFilter = vkb::Filter::Linear,
        .mipmapMode = vkb::SamplerMipmapMode::Linear,
        .compareEnable = true,
        .compareOp = vkb::CompareOp::Less,
        .borderColor = vkb::BorderColor::FloatOpaqueWhite,
    };
    VULL_ENSURE(vkCreateSampler(&shadow_sampler_ci, &m_shadow_sampler) == vkb::Result::Success);
    set_object_name(m_shadow_sampler, "Shadow sampler");
}

Context::~Context() {
    m_queues.clear();
    m_allocators.clear();
    vkDestroySampler(m_shadow_sampler);
    vkDestroySampler(m_depth_reduce_sampler);
    vkDestroySampler(m_linear_sampler);
    vkDestroySampler(m_nearest_sampler);
    vkDestroyDevice();
    vkDestroyDebugUtilsMessengerEXT(m_debug_utils_messenger);
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

Buffer Context::create_buffer(vkb::DeviceSize size, vkb::BufferUsage usage, MemoryUsage memory_usage) {
    VULL_ASSERT(size != 0);
    // TODO: Is it bad on any driver to always create a buffer with ShaderDeviceAddress?
    usage |= vkb::BufferUsage::ShaderDeviceAddress;
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
    return {vull::move(allocation), buffer, usage, size};
}

static vkb::ImageViewType pick_view_type(const vkb::ImageCreateInfo &image_ci) {
    if ((image_ci.flags & vkb::ImageCreateFlags::CubeCompatible) != vkb::ImageCreateFlags::None) {
        VULL_ASSERT(image_ci.arrayLayers == 6);
        return vkb::ImageViewType::Cube;
    }
    if (image_ci.arrayLayers > 1) {
        return vkb::ImageViewType::_2DArray;
    }
    return vkb::ImageViewType::_2D;
}

Image Context::create_image(const vkb::ImageCreateInfo &image_ci, MemoryUsage memory_usage) {
    vkb::Image image;
    VULL_ENSURE(vkCreateImage(&image_ci, &image) == vkb::Result::Success);

    vkb::MemoryRequirements requirements{};
    vkGetImageMemoryRequirements(image, &requirements);

    auto allocation = allocate_memory(requirements, memory_usage);
    const auto &info = allocation.info();
    VULL_ENSURE(vkBindImageMemory(image, info.memory, info.offset) == vkb::Result::Success);

    auto aspect = vkb::ImageAspect::Color;
    switch (image_ci.format) {
    case vkb::Format::D16Unorm:
    case vkb::Format::D16UnormS8Uint:
    case vkb::Format::D24UnormS8Uint:
    case vkb::Format::D32Sfloat:
    case vkb::Format::D32SfloatS8Uint:
        aspect = vkb::ImageAspect::Depth;
        break;
    }

    vkb::ImageSubresourceRange range{
        .aspectMask = aspect,
        .levelCount = image_ci.mipLevels,
        .layerCount = image_ci.arrayLayers,
    };
    vkb::ImageViewCreateInfo view_ci{
        .sType = vkb::StructureType::ImageViewCreateInfo,
        .image = image,
        .viewType = pick_view_type(image_ci),
        .format = image_ci.format,
        .subresourceRange = range,
    };
    vkb::ImageView view;
    VULL_ENSURE(vkCreateImageView(&view_ci, &view) == vkb::Result::Success);
    return {vull::move(allocation), image_ci.extent, image_ci.format, ImageView(this, image, view, range)};
}

Vector<Queue &> &Context::queue_list_for(QueueKind kind) {
    switch (kind) {
    case QueueKind::Compute:
        return m_compute_queues;
    case QueueKind::Graphics:
        return m_graphics_queues;
    case QueueKind::Transfer:
        return m_transfer_queues;
    default:
        vull::unreachable();
    }
}

QueueHandle Context::lock_queue(QueueKind kind) {
    // Try to pick a free queue.
    auto &queue_list = queue_list_for(kind);
    for (Queue &queue : queue_list) {
        if (queue.m_mutex.try_lock()) {
            return QueueHandle(queue);
        }
    }

    // Otherwise pick a random queue to wait on.
    VULL_ASSERT(!queue_list.empty());
    Queue &queue = queue_list[vull::linear_rand(0u, queue_list.size() - 1)];
    queue.m_mutex.lock();
    return QueueHandle(queue);
}

size_t Context::descriptor_size(vkb::DescriptorType type) const {
    switch (type) {
    case vkb::DescriptorType::Sampler:
        return m_descriptor_buffer_properties.samplerDescriptorSize;
    case vkb::DescriptorType::CombinedImageSampler:
        return m_descriptor_buffer_properties.combinedImageSamplerDescriptorSize;
    case vkb::DescriptorType::SampledImage:
        return m_descriptor_buffer_properties.sampledImageDescriptorSize;
    case vkb::DescriptorType::StorageImage:
        return m_descriptor_buffer_properties.storageImageDescriptorSize;
    case vkb::DescriptorType::UniformTexelBuffer:
        return m_descriptor_buffer_properties.uniformTexelBufferDescriptorSize;
    case vkb::DescriptorType::StorageTexelBuffer:
        return m_descriptor_buffer_properties.storageTexelBufferDescriptorSize;
    case vkb::DescriptorType::UniformBuffer:
        return m_descriptor_buffer_properties.uniformBufferDescriptorSize;
    case vkb::DescriptorType::StorageBuffer:
        return m_descriptor_buffer_properties.storageBufferDescriptorSize;
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

vkb::Sampler Context::get_sampler(Sampler sampler) const {
    switch (sampler) {
    case Sampler::None:
        return nullptr;
    case Sampler::Nearest:
        return m_nearest_sampler;
    case Sampler::Linear:
        return m_linear_sampler;
    case Sampler::DepthReduce:
        return m_depth_reduce_sampler;
    case Sampler::Shadow:
        return m_shadow_sampler;
    default:
        vull::unreachable();
    }
}

float Context::timestamp_elapsed(uint64_t start, uint64_t end) const {
    return (static_cast<float>(end - start) * m_properties.limits.timestampPeriod) / 1000000000.0f;
}

} // namespace vull::vk
