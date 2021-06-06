#include <vull/renderer/Shader.hh>

#include <vull/Config.hh>
#include <vull/renderer/Device.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Vector.hh>

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <fstream>

namespace {

Vector<char> load_binary(const std::string &path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    ENSURE(file);
    Vector<char> buffer(file.tellg());
    file.seekg(0);
    file.read(buffer.data(), buffer.capacity());
    return buffer;
}

} // namespace

Shader::Shader(const Device &device, const std::string &path) : m_device(device) {
    auto binary = load_binary(k_shader_path + path);
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
