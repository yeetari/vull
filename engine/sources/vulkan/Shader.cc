#include <vull/vulkan/Shader.hh>

#include <vull/support/Assert.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/Device.hh>

#include <spirv/unified1/spirv.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace {

VkShaderStageFlagBits shader_stage(SpvExecutionModel execution_model) {
    switch (execution_model) {
    case SpvExecutionModelVertex:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case SpvExecutionModelFragment:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    case SpvExecutionModelGLCompute:
        return VK_SHADER_STAGE_COMPUTE_BIT;
    default:
        ENSURE_NOT_REACHED();
    }
}

} // namespace

Shader::Shader(const Device &device, Vector<std::uint8_t> &&binary) : m_device(device) {
    const auto *code = reinterpret_cast<const std::uint32_t *>(binary.data());
    VkShaderModuleCreateInfo module_ci{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = binary.size(),
        .pCode = code,
    };
    ENSURE(vkCreateShaderModule(*device, &module_ci, nullptr, &m_module) == VK_SUCCESS);

    std::unordered_map<std::uint32_t, std::uint32_t> constant_ids;
    std::unordered_map<std::uint32_t, std::string> names;
    std::unordered_map<std::uint32_t, std::uint32_t> type_sizes;

    // Parse SPIR-V to extract push constant information.
    ENSURE(code[0] == SpvMagicNumber);
    const auto *inst = code + 5;
    while (inst != code + (binary.size() / 4)) {
        std::uint16_t opcode = (inst[0] >> 0u) & 0xffffu;
        std::uint16_t word_count = (inst[0] >> 16u) & 0xffffu;
        switch (opcode) {
        case SpvOpName: {
            ASSERT(word_count >= 3);
            std::string name;
            for (std::uint16_t i = 2; i < word_count; i++) {
                std::uint32_t word = inst[i];
                name += static_cast<char>((word >> 0u) & 0xffu);
                name += static_cast<char>((word >> 8u) & 0xffu);
                name += static_cast<char>((word >> 16u) & 0xffu);
                name += static_cast<char>((word >> 24u) & 0xffu);
            }
            name.erase(std::remove(name.begin(), name.end(), '\0'), name.end());
            names.emplace(inst[1], name);
            break;
        }
        case SpvOpEntryPoint:
            ASSERT(word_count >= 2);
            m_stage = shader_stage(static_cast<SpvExecutionModel>(inst[1]));
            break;
        case SpvOpTypeInt:
            ASSERT(word_count == 4);
            type_sizes[inst[1]] += inst[2] / 8;
            break;
        case SpvOpTypeFloat:
            ASSERT(word_count == 3);
            type_sizes[inst[1]] += inst[2] / 8;
            break;
        case SpvOpTypeVector:
        case SpvOpTypeMatrix:
        case SpvOpTypeArray:
            ASSERT(word_count == 4);
            type_sizes[inst[1]] += type_sizes[inst[2]] * inst[3];
            break;
        case SpvOpTypeStruct:
            ASSERT(word_count >= 3);
            for (std::uint16_t i = 2; i < word_count; i++) {
                type_sizes[inst[1]] += type_sizes[inst[i]];
            }
            break;
        case SpvOpTypePointer:
            ASSERT(word_count == 4);
            type_sizes[inst[1]] += type_sizes[inst[3]];
            break;
        case SpvOpSpecConstantTrue:
        case SpvOpSpecConstantFalse:
            ASSERT(word_count == 3);
            if (names[inst[2]].empty()) {
                break;
            }
            m_specialisation_constants.push(SpecialisationConstant{
                .name = names[inst[2]],
                .id = constant_ids[inst[2]],
                .size = type_sizes[inst[1]],
            });
            break;
        case SpvOpSpecConstant:
            ASSERT(word_count >= 4);
            if (names[inst[2]].empty()) {
                break;
            }
            m_specialisation_constants.push(SpecialisationConstant{
                .name = names[inst[2]],
                .id = constant_ids[inst[2]],
                .size = type_sizes[inst[1]],
            });
            break;
        case SpvOpVariable:
            ASSERT(word_count >= 4);
            if (inst[3] == SpvStorageClassPushConstant) {
                m_push_constant_size += type_sizes[inst[1]];
            }
            break;
        case SpvOpDecorate:
            ASSERT(word_count >= 3);
            if (inst[2] == SpvDecorationSpecId) {
                constant_ids.emplace(inst[1], inst[3]);
            }
            break;
        default:
            break;
        }
        inst += word_count;
    }
}

Shader::~Shader() {
    vkDestroyShaderModule(*m_device, m_module, nullptr);
}
