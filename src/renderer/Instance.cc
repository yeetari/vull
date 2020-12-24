#include <renderer/Instance.hh>

#include <support/Assert.hh>
#include <support/Vector.hh>

#include <algorithm>
#include <cstring>

namespace {

constexpr const char *APPLICATION_NAME = "vull";
constexpr std::uint32_t APPLICATION_VERSION = VK_MAKE_VERSION(0, 1, 0);
constexpr const char *ENGINE_NAME = "vull-engine";
constexpr std::uint32_t ENGINE_VERSION = VK_MAKE_VERSION(0, 1, 0);
constexpr const char *VALIDATION_LAYER_NAME = "VK_LAYER_KHRONOS_validation";

} // namespace

Instance::Instance(const Span<const char *> &extensions) {
    VkApplicationInfo application_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = APPLICATION_NAME,
        .applicationVersion = APPLICATION_VERSION,
        .pEngineName = ENGINE_NAME,
        .engineVersion = ENGINE_VERSION,
        .apiVersion = VK_API_VERSION_1_2,
    };
    VkInstanceCreateInfo instance_ci{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &application_info,
        .enabledExtensionCount = extensions.length(),
        .ppEnabledExtensionNames = extensions.data(),
    };
#ifndef NDEBUG
    std::uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    Vector<VkLayerProperties> layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, layers.data());
    auto *it = std::find_if(layers.begin(), layers.end(), [](const auto &layer) {
        return strcmp(layer.layerName, VALIDATION_LAYER_NAME) == 0;
    });
    ENSURE(it != layers.end());
    const char *layer_name = it->layerName;
    instance_ci.enabledLayerCount = 1;
    instance_ci.ppEnabledLayerNames = &layer_name;
#endif
    ENSURE(vkCreateInstance(&instance_ci, nullptr, &m_instance) == VK_SUCCESS);

    std::uint32_t physical_device_count = 0;
    vkEnumeratePhysicalDevices(m_instance, &physical_device_count, nullptr);
    m_physical_devices.relength(physical_device_count);
    vkEnumeratePhysicalDevices(m_instance, &physical_device_count, m_physical_devices.data());
    ENSURE(!m_physical_devices.empty());
}

Instance::~Instance() {
    vkDestroyInstance(m_instance, nullptr);
}
