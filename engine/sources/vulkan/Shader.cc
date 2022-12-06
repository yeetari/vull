#include <vull/vulkan/Shader.hh>

#include <vull/support/Array.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Result.hh>
#include <vull/support/Span.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Spirv.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull::vk {
namespace {

enum class Endian {
    Little,
    Big,
};

Endian determine_endian(Span<const uint8_t> magic) {
    for (uint32_t i = 0; i < 4; i++) {
        if (magic[i] != spv::k_magic_bytes[i]) {
            return Endian::Little;
        }
    }
    return Endian::Big;
}

vkb::ShaderStage to_shader_stage(spv::Word word) {
    switch (static_cast<spv::ExecutionModel>(word)) {
    case spv::ExecutionModel::Vertex:
        return vkb::ShaderStage::Vertex;
    case spv::ExecutionModel::Fragment:
        return vkb::ShaderStage::Fragment;
    case spv::ExecutionModel::GLCompute:
        return vkb::ShaderStage::Compute;
    default:
        return vkb::ShaderStage::All;
    }
}

} // namespace

Result<Shader, ShaderError> Shader::parse(const Context &context, Span<const uint8_t> data) {
    if (data.size() % sizeof(spv::Word) != 0) {
        return ShaderError::BadSize;
    }
    const uint32_t total_word_count = data.size() / sizeof(spv::Word);
    if (total_word_count < 5) {
        return ShaderError::BadSize;
    }

    const auto endian = determine_endian(data);
    Vector<spv::Word> words(total_word_count);
    for (uint32_t i = 0; i < total_word_count; i++) {
        spv::Word &word = words[i];
        if (endian == Endian::Big) {
            word |= static_cast<spv::Word>(data[i * sizeof(spv::Word) + 0] << 24u);
            word |= static_cast<spv::Word>(data[i * sizeof(spv::Word) + 1] << 16u);
            word |= static_cast<spv::Word>(data[i * sizeof(spv::Word) + 2] << 8u);
            word |= static_cast<spv::Word>(data[i * sizeof(spv::Word) + 3] << 0u);
        } else {
            word |= static_cast<spv::Word>(data[i * sizeof(spv::Word) + 0] << 0u);
            word |= static_cast<spv::Word>(data[i * sizeof(spv::Word) + 1] << 8u);
            word |= static_cast<spv::Word>(data[i * sizeof(spv::Word) + 2] << 16u);
            word |= static_cast<spv::Word>(data[i * sizeof(spv::Word) + 3] << 24u);
        }
    }

    if (words[0] != spv::k_magic_number) {
        return ShaderError::BadMagic;
    }

    bool seen_entry_point = false;
    auto shader_stage = vkb::ShaderStage::All;
    for (uint32_t i = 5; i < words.size();) {
        uint16_t opcode = (words[i] >> 0u) & 0xffffu;
        uint16_t word_count = (words[i] >> 16u) & 0xffffu;
        switch (static_cast<spv::Op>(opcode)) {
        case spv::Op::EntryPoint:
            if (i + word_count >= words.size() || word_count < 4) {
                return ShaderError::Malformed;
            }
            if (seen_entry_point) {
                return ShaderError::MultipleEntryPoints;
            }
            seen_entry_point = true;
            shader_stage = to_shader_stage(words[i + 1]);
            break;
        }
        i += word_count;
    }

    vkb::ShaderModuleCreateInfo module_ci{
        .sType = vkb::StructureType::ShaderModuleCreateInfo,
        .codeSize = words.size_bytes(),
        .pCode = words.data(),
    };
    vkb::ShaderModule module;
    if (context.vkCreateShaderModule(&module_ci, &module) != vkb::Result::Success) {
        return ShaderError::ModuleCreation;
    }
    return Shader(context, module, shader_stage);
}

Shader::Shader(Shader &&other) : m_context(other.m_context) {
    m_module = vull::exchange(other.m_module, nullptr);
    m_stage = vull::exchange(other.m_stage, {});
}

Shader::~Shader() {
    m_context.vkDestroyShaderModule(m_module);
}

vkb::PipelineShaderStageCreateInfo Shader::create_info(Optional<const vkb::SpecializationInfo &> si) const {
    return {
        .sType = vkb::StructureType::PipelineShaderStageCreateInfo,
        .stage = m_stage,
        .module = m_module,
        .pName = "main",
        .pSpecializationInfo = si ? &*si : nullptr,
    };
}

} // namespace vull::vk
