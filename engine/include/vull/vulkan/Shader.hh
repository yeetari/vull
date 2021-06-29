#pragma once

#include <vull/support/Vector.hh>

#include <vulkan/vulkan_core.h>

#include <cstdint>

class Device;

class Shader {
    const Device &m_device;
    VkShaderModule m_module{nullptr};

    VkShaderStageFlagBits m_stage;
    std::uint32_t m_push_constant_size{0};

public:
    Shader(const Device &device, Vector<std::uint8_t> &&binary);
    Shader(const Shader &) = delete;
    Shader(Shader &&) = delete;
    ~Shader();

    Shader &operator=(const Shader &) = delete;
    Shader &operator=(Shader &&) = delete;

    VkShaderModule operator*() const { return m_module; }
    VkShaderStageFlagBits stage() const { return m_stage; }
    std::uint32_t push_constant_size() const { return m_push_constant_size; }
};
