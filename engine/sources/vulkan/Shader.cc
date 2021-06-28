#include <vull/vulkan/Shader.hh>

#include <vull/support/Assert.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/Device.hh>

#include <vulkan/vulkan_core.h>

#include <cstdint>

Shader::Shader(const Device &device, const Vector<std::uint8_t> &binary) : m_device(device) {
    VkShaderModuleCreateInfo module_ci{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = binary.size(),
        .pCode = reinterpret_cast<const std::uint32_t *>(binary.data()),
    };
    ENSURE(vkCreateShaderModule(*device, &module_ci, nullptr, &m_module) == VK_SUCCESS);
}

Shader::~Shader() {
    vkDestroyShaderModule(*m_device, m_module, nullptr);
}
