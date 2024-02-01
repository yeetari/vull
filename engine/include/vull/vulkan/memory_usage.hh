#pragma once

namespace vull::vk {

enum class MemoryUsage {
    // Guarantees DEVICE_LOCAL.
    DeviceOnly,
    // Guarantees HOST_VISIBLE|HOST_COHERENT, prefers not DEVICE_LOCAL.
    HostOnly,
    // Guarantees HOST_VISIBLE|HOST_COHERENT, prefers DEVICE_LOCAL.
    HostToDevice,
    // Guarantees HOST_VISIBLE, prefers HOST_CACHED.
    DeviceToHost,
};

} // namespace vull::vk
