#pragma once

#include <vull/support/Optional.hh>
#include <vull/support/Result.hh>
#include <vull/support/Span.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Context;

enum class ShaderError {
    BadMagic,
    BadSize,
    Malformed,
    MultipleEntryPoints,
    ModuleCreation,
};

class Shader {
    const Context &m_context;
    vkb::ShaderModule m_module{nullptr};
    vkb::ShaderStage m_stage{};

    Shader(const Context &context, vkb::ShaderModule module, vkb::ShaderStage stage)
        : m_context(context), m_module(module), m_stage(stage) {}

public:
    static Result<Shader, ShaderError> parse(const Context &context, Span<const uint8_t> data);
    Shader(const Shader &) = delete;
    Shader(Shader &&);
    ~Shader();

    Shader &operator=(const Shader &) = delete;
    Shader &operator=(Shader &&) = delete;

    vkb::PipelineShaderStageCreateInfo create_info(Optional<const vkb::SpecializationInfo &> si = {}) const;
    vkb::ShaderModule module() const { return m_module; }
    vkb::ShaderStage stage() const { return m_stage; }
};

} // namespace vull::vk
