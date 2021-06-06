#pragma once

#include <vulkan/vulkan_core.h>

#include <string>

class Device;

class Shader {
    const Device &m_device;
    VkShaderModule m_module{nullptr};

public:
    Shader(const Device &device, const std::string &path);
    Shader(const Shader &) = delete;
    Shader(Shader &&) = delete;
    ~Shader();

    Shader &operator=(const Shader &) = delete;
    Shader &operator=(Shader &&) = delete;

    VkShaderModule operator*() const { return m_module; }
};
