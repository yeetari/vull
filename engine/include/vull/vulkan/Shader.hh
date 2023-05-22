#pragma once

#include <vull/container/Vector.hh>
#include <vull/support/Result.hh>
#include <vull/support/Span.hh>
#include <vull/support/StreamError.hh>
#include <vull/support/StringView.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Context;

enum class ShaderError {
    BadMagic,
    BadSize,
    BadVersion,
    Malformed,
    ModuleCreation,
    MultipleEntryPoints,
    NoEntryPoint,
    Unhandled,
};

class Shader {
    const Context *m_context{nullptr};
    vkb::ShaderModule m_module{nullptr};
    vkb::ShaderStage m_stage{vkb::ShaderStage::All};
    Vector<vkb::VertexInputAttributeDescription> m_vertex_attributes;
    uint32_t m_vertex_stride{0};

    explicit Shader(const Context &context) : m_context(&context) {}
    void set_module(vkb::ShaderModule module) { m_module = module; }
    void set_stage(vkb::ShaderStage stage) { m_stage = stage; }
    void add_vertex_attribute(uint32_t location, vkb::Format format);
    void set_vertex_stride(uint32_t stride) { m_vertex_stride = stride; }
    Vector<vkb::VertexInputAttributeDescription> &vertex_attributes() { return m_vertex_attributes; }

public:
    static Result<Shader, ShaderError> parse(const Context &context, Span<const uint8_t> data);
    static Result<Shader, ShaderError, StreamError> load(const Context &context, StringView name);
    Shader(const Shader &) = delete;
    Shader(Shader &&);
    ~Shader();

    Shader &operator=(const Shader &) = delete;
    Shader &operator=(Shader &&);

    vkb::ShaderModule module() const { return m_module; }
    vkb::ShaderStage stage() const { return m_stage; }
    const Vector<vkb::VertexInputAttributeDescription> &vertex_attributes() const { return m_vertex_attributes; }
    uint32_t vertex_stride() const { return m_vertex_stride; }
};

} // namespace vull::vk
