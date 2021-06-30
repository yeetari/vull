#pragma once

#include <vull/support/Vector.hh>

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <string>

class Device;

class Shader {
    struct SpecialisationConstant {
        std::string name;
        std::uint32_t id;
        std::uint32_t size;
    };

    const Device &m_device;
    VkShaderModule m_module{nullptr};

    VkShaderStageFlagBits m_stage;
    std::uint32_t m_push_constant_size{0};
    Vector<SpecialisationConstant> m_specialisation_constants;

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
    const Vector<SpecialisationConstant> &specialisation_constants() const { return m_specialisation_constants; }
};
