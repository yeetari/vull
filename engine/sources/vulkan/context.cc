#include <vull/vulkan/context.hh>

#include <vull/container/array.hh>
#include <vull/container/vector.hh>
#include <vull/core/config.hh>
#include <vull/core/log.hh>
#include <vull/core/tracing.hh>
#include <vull/maths/common.hh>
#include <vull/support/assert.hh>
#include <vull/support/enum.hh>
#include <vull/support/optional.hh>
#include <vull/support/result.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/vulkan/buffer.hh>
#include <vull/vulkan/context_table.hh>
#include <vull/vulkan/fence.hh>
#include <vull/vulkan/image.hh>
#include <vull/vulkan/memory.hh>
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

constexpr uint32_t make_version(uint32_t variant, uint32_t major, uint32_t minor, uint32_t patch) {
    return (variant << 29) | (major << 22) | (minor << 12) | patch;
}

constexpr uint32_t make_version(EngineVersion version) {
    return make_version(0, version.major, version.minor, version.patch);
}

constexpr uint32_t k_vulkan_version = make_version(0, 1, 3, 0);

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

    sb.append("\n{}", callback_data->pMessage);

    if (severity == vkb::DebugUtilsMessageSeverityFlagsEXT::Warning) {
        vull::warn("[vulkan] Validation warning: {}", sb.build());
        return false;
    }

    vull::error("[vulkan] Validation error: {}", sb.build());
    vull::close_log();
    abort();
}

template <typename T, typename F>
Vector<T> build_vector(F &&function) {
    uint32_t count = 0;
    function(&count, nullptr);
    Vector<T> vector(count);
    if constexpr (vull::is_same<T, vkb::QueueFamilyProperties2>) {
        for (auto &properties : vector) {
            properties.sType = vkb::StructureType::QueueFamilyProperties2;
        }
    }
    function(&count, vector.data());
    return vector;
}

} // namespace

Result<UniquePtr<Context>, ContextError> Context::create(const AppInfo &app_info) {
    // Try to open loader shared library.
    void *loader_module = dlopen("libvulkan.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (loader_module == nullptr) {
        loader_module = dlopen("libvulkan.so", RTLD_LAZY | RTLD_LOCAL);
    }
    if (loader_module == nullptr) {
        return ContextError::LoaderUnavailable;
    }

    auto *vkGetInstanceProcAddr =
        vull::bit_cast<vkb::PFN_vkGetInstanceProcAddr>(dlsym(loader_module, "vkGetInstanceProcAddr"));
    if (vkGetInstanceProcAddr == nullptr) {
        return ContextError::LoaderUnavailable;
    }

    // Load loader (non-instance specific) functions.
    vkb::ContextTable context_table;
    if (!context_table.load_loader(vkGetInstanceProcAddr)) {
        // Loader only supports vulkan 1.0.
        return ContextError::VersionUnsupported;
    }

    // Even though this function is called EnumerateInstanceVersion, it effectively gets the version of vulkan the
    // loader itself knows about.
    uint32_t loader_version;
    if (context_table.vkEnumerateInstanceVersion(&loader_version) != vkb::Result::Success) {
        return ContextError::VersionUnsupported;
    }
    if (loader_version < k_vulkan_version) {
        return ContextError::VersionUnsupported;
    }

    // Get available layers and extensions.
    const auto layer_properties = build_vector<vkb::LayerProperties>([&](auto... args) {
        return context_table.vkEnumerateInstanceLayerProperties(args...);
    });
    const auto extension_properties = build_vector<vkb::ExtensionProperties>([&](auto... args) {
        return context_table.vkEnumerateInstanceExtensionProperties(nullptr, args...);
    });

    // Check if validation layer is available.
    bool validation_layer_available = false;
    const char *validation_layer_name = "VK_LAYER_KHRONOS_validation";
    for (const auto &layer : layer_properties) {
        if (StringView(static_cast<const char *>(layer.layerName)) == validation_layer_name) {
            validation_layer_available = true;
            break;
        }
    }
    if (app_info.enable_validation && !validation_layer_available) {
        vull::warn("[vulkan] Validation layer not available");
    }

    // Check instance extension support.
    Vector<const char *> instance_extensions;
    instance_extensions.extend(app_info.instance_extensions);
    instance_extensions.push("VK_EXT_debug_utils");
    for (const char *desired_extension : instance_extensions) {
        bool found = false;
        for (const auto &extension : extension_properties) {
            if (StringView(static_cast<const char *>(extension.extensionName)) == desired_extension) {
                found = true;
                break;
            }
        }

        if (!found) {
            vull::error("[vulkan] Instance extension {} not supported", desired_extension);
            return ContextError::InstanceExtensionUnsupported;
        }
    }

    const char *event_vuid = "VUID-vkCmdWaitEvents2-pEvents-10788";
    const uint32_t true_value = 1;
    Array layer_settings{
        vkb::LayerSettingEXT{
            validation_layer_name,
            "validate_core",
            vkb::LayerSettingTypeEXT::Bool32,
            1,
            &true_value,
        },
        vkb::LayerSettingEXT{
            validation_layer_name,
            "validate_sync",
            vkb::LayerSettingTypeEXT::Bool32,
            1,
            &true_value,
        },
        // TODO: The render graph events are currently not correct.
        vkb::LayerSettingEXT{
            validation_layer_name,
            "message_id_filter",
            vkb::LayerSettingTypeEXT::String,
            1,
            &event_vuid,
        },
    };
    vkb::LayerSettingsCreateInfoEXT layer_settings_ci{
        .sType = vkb::StructureType::LayerSettingsCreateInfoEXT,
        .settingCount = layer_settings.size(),
        .pSettings = layer_settings.data(),
    };
    vkb::ApplicationInfo application_info{
        .sType = vkb::StructureType::ApplicationInfo,
        .pApplicationName = app_info.name,
        .applicationVersion = app_info.version,
        .pEngineName = "vull",
        .engineVersion = make_version(vull::engine_version()),
        .apiVersion = k_vulkan_version,
    };
    vkb::InstanceCreateInfo instance_ci{
        .sType = vkb::StructureType::InstanceCreateInfo,
        .pNext = &layer_settings_ci,
        .pApplicationInfo = &application_info,
        .enabledExtensionCount = instance_extensions.size(),
        .ppEnabledExtensionNames = instance_extensions.data(),
    };
    if (app_info.enable_validation && validation_layer_available) {
        vull::info("[vulkan] Enabling validation layer");
        instance_ci.enabledLayerCount = 1;
        instance_ci.ppEnabledLayerNames = &validation_layer_name;
    }
    vkb::Instance instance;
    if (context_table.vkCreateInstance(&instance_ci, &instance) != vkb::Result::Success) {
        // This should never really happen since we check extension and layer support. We also don't need to worry about
        // ErrorIncompatibleDriver since that can only happen on 1.0 drivers.
        return ContextError::InstanceCreationFailed;
    }
    context_table.load_instance(instance, vkGetInstanceProcAddr);

    // Create a debug utils messenger for validation errors.
    vkb::DebugUtilsMessengerCreateInfoEXT debug_utils_messenger_ci{
        .sType = vkb::StructureType::DebugUtilsMessengerCreateInfoEXT,
        .messageSeverity =
            vkb::DebugUtilsMessageSeverityFlagsEXT::Warning | vkb::DebugUtilsMessageSeverityFlagsEXT::Error,
        .messageType = vkb::DebugUtilsMessageTypeFlagsEXT::Validation,
        .pfnUserCallback = &validation_callback,
    };
    vkb::DebugUtilsMessengerEXT debug_utils_messenger;
    if (context_table.vkCreateDebugUtilsMessengerEXT(&debug_utils_messenger_ci, &debug_utils_messenger) !=
        vkb::Result::Success) {
        return ContextError::Unknown;
    }

    // Pick the first physical device.
    // TODO: Prioritise dedicated hardware and allow override.
    vkb::PhysicalDevice physical_device;
    uint32_t physical_device_count = 1;
    const auto enumeration_result = context_table.vkEnumeratePhysicalDevices(&physical_device_count, &physical_device);
    if (enumeration_result != vkb::Result::Success && enumeration_result != vkb::Result::Incomplete) {
        return ContextError::NoSuitableDevice;
    }
    context_table.set_physical_device(physical_device);

    // Check that syncfd fence export is supported.
    vkb::PhysicalDeviceExternalFenceInfo external_fence_info{
        .sType = vkb::StructureType::PhysicalDeviceExternalFenceInfo,
        .handleType = vkb::ExternalFenceHandleTypeFlags::SyncFd,
    };
    vkb::ExternalFenceProperties external_fence_properties{
        .sType = vkb::StructureType::ExternalFenceProperties,
    };
    context_table.vkGetPhysicalDeviceExternalFenceProperties(&external_fence_info, &external_fence_properties);
    if ((external_fence_properties.externalFenceFeatures & vkb::ExternalFenceFeature::Exportable) !=
        vkb::ExternalFenceFeature::Exportable) {
        vull::error("[vulkan] Fence export to syncfd unsupported");
        return ContextError::DeviceFeatureUnsupported;
    }

    // Get supported features.
    vkb::PhysicalDeviceVulkan11Features supported_features_11{
        .sType = vkb::StructureType::PhysicalDeviceVulkan11Features,
    };
    vkb::PhysicalDeviceVulkan12Features supported_features_12{
        .sType = vkb::StructureType::PhysicalDeviceVulkan12Features,
        .pNext = &supported_features_11,
    };
    vkb::PhysicalDeviceShaderAtomicFloat2FeaturesEXT supported_features_atomic_float_2{
        .sType = vkb::StructureType::PhysicalDeviceShaderAtomicFloat2FeaturesEXT,
        .pNext = &supported_features_12,
    };
    vkb::PhysicalDeviceDescriptorBufferFeaturesEXT supported_features_descriptor_buffer{
        .sType = vkb::StructureType::PhysicalDeviceDescriptorBufferFeaturesEXT,
        .pNext = &supported_features_atomic_float_2,
    };
    vkb::PhysicalDeviceFeatures2 supported_features{
        .sType = vkb::StructureType::PhysicalDeviceFeatures2,
        .pNext = &supported_features_descriptor_buffer,
    };
    context_table.vkGetPhysicalDeviceFeatures2(&supported_features);

    // Check 1.0 features.
    if (!supported_features.features.multiDrawIndirect) {
        vull::error("[vulkan] Feature multiDrawIndirect not supported");
        return ContextError::DeviceFeatureUnsupported;
    }
    const bool anisotropy_supported = supported_features.features.samplerAnisotropy;
    if (!anisotropy_supported) {
        vull::warn("[vulkan] Feature samplerAnisotropy not supported");
    }
    if (!supported_features.features.textureCompressionBC) {
        vull::error("[vulkan] Feature textureCompressionBC not supported");
        return ContextError::DeviceFeatureUnsupported;
    }
    if (!supported_features.features.pipelineStatisticsQuery) {
        // TODO: This shouldn't be required.
        vull::error("[vulkan] Feature pipelineStatisticsQuery not supported");
        return ContextError::DeviceFeatureUnsupported;
    }
    if (!supported_features.features.shaderInt16) {
        vull::error("[vulkan] Feature shaderInt16 not supported");
        return ContextError::DeviceFeatureUnsupported;
    }

    // Check 1.1 features.
    if (!supported_features_11.storageBuffer16BitAccess) {
        vull::error("[vulkan] Feature storageBuffer16BitAccess not supported");
        return ContextError::DeviceFeatureUnsupported;
    }
    if (!supported_features_11.shaderDrawParameters) {
        vull::error("[vulkan] Feature shaderDrawParameters not supported");
        return ContextError::DeviceFeatureUnsupported;
    }

    // Check 1.2 features.
    if (!supported_features_12.drawIndirectCount) {
        vull::error("[vulkan] Feature drawIndirectCount not supported");
        return ContextError::DeviceFeatureUnsupported;
    }
    if (!supported_features_12.descriptorIndexing) {
        vull::error("[vulkan] Feature descriptorIndexing not supported");
        return ContextError::DeviceFeatureUnsupported;
    }
    if (!supported_features_12.descriptorBindingVariableDescriptorCount) {
        vull::error("[vulkan] Feature descriptorBindingVariableDescriptorCount not supported");
        return ContextError::DeviceFeatureUnsupported;
    }
    if (!supported_features_12.samplerFilterMinmax) {
        vull::error("[vulkan] Feature samplerFilterMinmax not supported");
        return ContextError::DeviceFeatureUnsupported;
    }
    if (!supported_features_12.scalarBlockLayout) {
        vull::error("[vulkan] Feature scalarBlockLayout not supported");
        return ContextError::DeviceFeatureUnsupported;
    }

    // Check extension features.
    if (!supported_features_atomic_float_2.shaderSharedFloat32AtomicMinMax) {
        vull::error("[vulkan] Feature shaderSharedFloat32AtomicMinMax not supported");
        return ContextError::DeviceFeatureUnsupported;
    }
    if (!supported_features_descriptor_buffer.descriptorBuffer) {
        vull::error("[vulkan] Feature descriptorBuffer not supported");
        return ContextError::DeviceFeatureUnsupported;
    }

    vkb::PhysicalDeviceVulkan11Features requested_features_11{
        .sType = vkb::StructureType::PhysicalDeviceVulkan11Features,
        .storageBuffer16BitAccess = true,
        .multiview = true, // Guaranteed
        .shaderDrawParameters = true,
    };
    vkb::PhysicalDeviceVulkan12Features requested_features_12{
        .sType = vkb::StructureType::PhysicalDeviceVulkan12Features,
        .pNext = &requested_features_11,
        .drawIndirectCount = true,

        // Guaranteed by the descriptorIndexing feature flag. We don't care about any of the UpdateAfterBind features
        // since we're using descriptor buffers.
        .descriptorIndexing = true,
        .shaderUniformTexelBufferArrayDynamicIndexing = true,
        .shaderStorageTexelBufferArrayDynamicIndexing = true,
        .shaderSampledImageArrayNonUniformIndexing = true,
        .shaderStorageBufferArrayNonUniformIndexing = true,
        .shaderUniformTexelBufferArrayNonUniformIndexing = true,
        .descriptorBindingUpdateUnusedWhilePending = true,
        .descriptorBindingPartiallyBound = true,

        .descriptorBindingVariableDescriptorCount = true,

        // Guaranteed by the descriptorIndexing feature flag.
        .runtimeDescriptorArray = true,

        .samplerFilterMinmax = true,
        .scalarBlockLayout = true,

        // Guaranteed.
        .uniformBufferStandardLayout = true,
        .shaderSubgroupExtendedTypes = true,
        .separateDepthStencilLayouts = true,
        .hostQueryReset = true,
        .timelineSemaphore = true,

        // Guaranteed in 1.3 and above.
        .bufferDeviceAddress = true,
        .vulkanMemoryModel = true,
        .vulkanMemoryModelDeviceScope = true,

        // Guaranteed.
        .subgroupBroadcastDynamicId = true,
    };
    vkb::PhysicalDeviceVulkan13Features requested_features_13{
        .sType = vkb::StructureType::PhysicalDeviceVulkan13Features,
        .pNext = &requested_features_12,

        // All features guaranteed by spec.
        .inlineUniformBlock = true,
        .shaderDemoteToHelperInvocation = true,
        .shaderTerminateInvocation = true,
        .subgroupSizeControl = true,
        .computeFullSubgroups = true,
        .synchronization2 = true,
        .shaderZeroInitializeWorkgroupMemory = true,
        .dynamicRendering = true,
        .shaderIntegerDotProduct = true,
        .maintenance4 = true,
    };
    vkb::PhysicalDeviceShaderAtomicFloat2FeaturesEXT requested_features_atomic_float_2{
        .sType = vkb::StructureType::PhysicalDeviceShaderAtomicFloat2FeaturesEXT,
        .pNext = &requested_features_13,
        .shaderSharedFloat32AtomicMinMax = true,
    };
    vkb::PhysicalDeviceDescriptorBufferFeaturesEXT requested_features_descriptor_buffer{
        .sType = vkb::StructureType::PhysicalDeviceDescriptorBufferFeaturesEXT,
        .pNext = &requested_features_atomic_float_2,
        .descriptorBuffer = true,
    };
    vkb::PhysicalDeviceFeatures2 requested_features{
        .sType = vkb::StructureType::PhysicalDeviceFeatures2,
        .pNext = &requested_features_descriptor_buffer,
        .features{
            // Supported everywhere.
            .fullDrawIndexUint32 = true,
            .imageCubeArray = true,
            .independentBlend = true,

            .multiDrawIndirect = true,
            .samplerAnisotropy = anisotropy_supported,
            .textureCompressionBC = true,
            .pipelineStatisticsQuery = true,

            // Supported everywhere.
            .fragmentStoresAndAtomics = true,

            // Guaranteed by descriptorIndexing 1.2 flag.
            .shaderSampledImageArrayDynamicIndexing = true,
            .shaderStorageBufferArrayDynamicIndexing = true,

            .shaderInt16 = true,
        },
    };

    // Get physical device properties.
    // TODO: Check limits and format features.
    vkb::PhysicalDeviceProperties2 device_properties{
        .sType = vkb::StructureType::PhysicalDeviceProperties2,
    };
    context_table.vkGetPhysicalDeviceProperties2(&device_properties);
    vull::info("[vulkan] Using {}", device_properties.properties.deviceName);

    // Get queue family info.
    const auto queue_families = build_vector<vkb::QueueFamilyProperties2>([&](auto... args) {
        return context_table.vkGetPhysicalDeviceQueueFamilyProperties2(args...);
    });
    uint32_t max_queue_count = 0;
    for (const auto &family : queue_families) {
        max_queue_count = vull::max(max_queue_count, family.queueFamilyProperties.queueCount);
    }

    // Build queue create infos.
    Vector<vkb::DeviceQueueCreateInfo> queue_cis;
    Vector<float> queue_priorities(max_queue_count, 1.0f);
    for (uint32_t i = 0; i < queue_families.size(); i++) {
        const auto &properties = queue_families[i].queueFamilyProperties;

        if ((properties.queueFlags & (vkb::QueueFlags::Graphics | vkb::QueueFlags::Compute |
                                      vkb::QueueFlags::Transfer)) == vkb::QueueFlags::None) {
            continue;
        }

        queue_cis.push({
            .sType = vkb::StructureType::DeviceQueueCreateInfo,
            .queueFamilyIndex = i,
            .queueCount = properties.queueCount,
            .pQueuePriorities = queue_priorities.data(),
        });
        vull::debug("[vulkan] Creating {} queues from family {}", properties.queueCount, i);
    }

    Array device_extensions{
        "VK_EXT_descriptor_buffer", "VK_EXT_shader_atomic_float", "VK_EXT_shader_atomic_float2",
        "VK_KHR_external_fence_fd", "VK_KHR_swapchain",
    };
    vkb::DeviceCreateInfo device_ci{
        .sType = vkb::StructureType::DeviceCreateInfo,
        .pNext = &requested_features,
        .queueCreateInfoCount = queue_cis.size(),
        .pQueueCreateInfos = queue_cis.data(),
        .enabledExtensionCount = device_extensions.size(),
        .ppEnabledExtensionNames = device_extensions.data(),
    };
    vkb::Device device;
    if (context_table.vkCreateDevice(&device_ci, &device) != vkb::Result::Success) {
        return ContextError::DeviceCreationFailed;
    }
    context_table.load_device(device);
    return vull::make_unique<Context>(context_table, queue_families, debug_utils_messenger, anisotropy_supported);
}

template <vkb::ObjectType ObjectType>
void Context::set_object_name(const void *object, StringView name) const {
    vkb::DebugUtilsObjectNameInfoEXT name_info{
        .sType = vkb::StructureType::DebugUtilsObjectNameInfoEXT,
        .objectType = ObjectType,
        .objectHandle = vull::bit_cast<uint64_t>(object),
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

Context::Context(const vkb::ContextTable &table, const Vector<vkb::QueueFamilyProperties2> &queue_families,
                 vkb::DebugUtilsMessengerEXT debug_utils_messenger, bool anisotropy_supported)
    : vkb::ContextTable(table), m_debug_utils_messenger(debug_utils_messenger) {
    m_descriptor_buffer_properties.sType = vkb::StructureType::PhysicalDeviceDescriptorBufferPropertiesEXT;
    vkb::PhysicalDeviceProperties2 properties{
        .sType = vkb::StructureType::PhysicalDeviceProperties2,
        .pNext = &m_descriptor_buffer_properties,
    };
    vkGetPhysicalDeviceProperties2(&properties);
    m_properties = properties.properties;

    m_allocator = vull::make_unique<DeviceMemoryAllocator>(*this);
    for (uint32_t family_index = 0; family_index < queue_families.size(); family_index++) {
        const auto &family = queue_families[family_index].queueFamilyProperties;
        const auto flags = family.queueFlags;
        if ((flags & (vkb::QueueFlags::Graphics | vkb::QueueFlags::Compute | vkb::QueueFlags::Transfer)) ==
            vkb::QueueFlags::None) {
            continue;
        }

        auto *queue = m_queues.emplace(new Queue(*this, family_index, family.queueCount)).ptr();
        if ((flags & vkb::QueueFlags::Graphics) != vkb::QueueFlags::None) {
            m_graphics_queue = queue;
        } else if ((flags & vkb::QueueFlags::Compute) != vkb::QueueFlags::None) {
            m_compute_queue = queue;
        } else if ((flags & vkb::QueueFlags::Transfer) != vkb::QueueFlags::None) {
            m_transfer_queue = queue;
        }
    }

    VULL_ENSURE(m_graphics_queue != nullptr);
    if (m_compute_queue == nullptr) {
        m_compute_queue = m_graphics_queue;
    }
    if (m_transfer_queue == nullptr) {
        m_transfer_queue = m_compute_queue;
    }

    // TODO: Create a sampler cache.
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
        .anisotropyEnable = anisotropy_supported,
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
    m_allocator.clear();
    vkDestroySampler(m_shadow_sampler);
    vkDestroySampler(m_depth_reduce_sampler);
    vkDestroySampler(m_linear_sampler);
    vkDestroySampler(m_nearest_sampler);
    vkDestroyDevice();
    vkDestroyDebugUtilsMessengerEXT(m_debug_utils_messenger);
    vkDestroyInstance();
}

Buffer Context::create_buffer(vkb::DeviceSize size, vkb::BufferUsage usage, DeviceMemoryFlags memory_flags) {
    VULL_ASSERT(size != 0);
    tracing::ScopedTrace trace("Create VkBuffer");

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

    auto allocation = m_allocator->allocate_for(buffer, memory_flags);
    VULL_ENSURE(allocation);
    return {vull::move(*allocation), buffer, usage, size};
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

Image Context::create_image(const vkb::ImageCreateInfo &image_ci, DeviceMemoryFlags memory_flags) {
    tracing::ScopedTrace trace("Create VkImage");
    vkb::Image image;
    VULL_ENSURE(vkCreateImage(&image_ci, &image) == vkb::Result::Success);

    auto allocation = m_allocator->allocate_for(image, memory_flags);
    VULL_ENSURE(allocation);

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
    return {vull::move(*allocation), image_ci.extent, image_ci.format, ImageView(this, image, view, range)};
}

Queue &Context::get_queue(QueueKind kind) {
    switch (kind) {
    case QueueKind::Compute:
        return *m_compute_queue;
    case QueueKind::Graphics:
        return *m_graphics_queue;
    case QueueKind::Transfer:
        return *m_transfer_queue;
    default:
        vull::unreachable();
    }
}

void Context::wait_idle() const {
    for (const auto &queue : m_queues) {
        queue->wait_idle();
    }
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
