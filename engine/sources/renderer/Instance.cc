#include <vull/renderer/Instance.hh>

#include <vull/support/Assert.hh>
#include <vull/support/Log.hh>
#include <vull/support/Span.hh>
#include <vull/support/Vector.hh>

#include <algorithm>
#include <cstdint>
#include <cstring>

#define VULL_MAKE_VERSION(major, minor, patch)                                                                         \
    ((static_cast<std::uint32_t>(major) << 22u) | (static_cast<std::uint32_t>(minor) << 12u) |                         \
     static_cast<std::uint32_t>(patch))

namespace {

constexpr const char *k_application_name = "vull";
constexpr std::uint32_t k_application_version = VULL_MAKE_VERSION(0, 1, 0);
constexpr const char *k_engine_name = "vull-engine";
constexpr std::uint32_t k_engine_version = VULL_MAKE_VERSION(0, 1, 0);
constexpr const char *k_validation_layer_name = "VK_LAYER_KHRONOS_validation";

} // namespace

Instance::Instance(Span<const char *> extensions) {
    Log::trace("renderer", "Creating vulkan instance with %d extensions", extensions.size());
    for (const char *extension : extensions) {
        Log::trace("renderer", " - %s", extension);
    }
    VkApplicationInfo application_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = k_application_name,
        .applicationVersion = k_application_version,
        .pEngineName = k_engine_name,
        .engineVersion = k_engine_version,
        .apiVersion = VULL_MAKE_VERSION(1, 2, 0),
    };
    VkInstanceCreateInfo instance_ci{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &application_info,
        .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };
#ifndef NDEBUG
    Log::debug("renderer", "Enabling vulkan validation layers");
    std::uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    Vector<VkLayerProperties> layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, layers.data());
    auto *it = std::find_if(layers.begin(), layers.end(), [](const auto &layer) {
        return strcmp(static_cast<const char *>(layer.layerName), k_validation_layer_name) == 0;
    });
    const auto *layer_name = static_cast<const char *>(it->layerName);
    if (it != layers.end()) {
        instance_ci.enabledLayerCount = 1;
        instance_ci.ppEnabledLayerNames = &layer_name;
    } else {
        Log::warn("renderer", "Failed to enable validation layers!");
    }
#endif
    ENSURE(vkCreateInstance(&instance_ci, nullptr, &m_instance) == VK_SUCCESS);

    std::uint32_t physical_device_count = 0;
    vkEnumeratePhysicalDevices(m_instance, &physical_device_count, nullptr);
    m_physical_devices.resize(physical_device_count);
    vkEnumeratePhysicalDevices(m_instance, &physical_device_count, m_physical_devices.data());
    ENSURE(!m_physical_devices.empty());
}

Instance::~Instance() {
    vkDestroyInstance(m_instance, nullptr);
}
