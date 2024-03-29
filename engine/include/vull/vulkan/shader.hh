#pragma once

#include <vull/container/vector.hh>
#include <vull/support/result.hh>
#include <vull/support/span.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/vulkan/spirv.hh>
#include <vull/vulkan/vulkan.hh>

#include <stdint.h>

namespace vull {

enum class StreamError;

} // namespace vull

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
    struct ConstantInfo {
        String name;
        spv::Id id;
        uint32_t size;
    };

    struct EntryPoint {
        String name;
        vkb::ShaderStage stage;
        Vector<spv::Id> interface_ids;
    };

private:
    const Context *m_context{nullptr};
    vkb::ShaderModule m_module{nullptr};
    Vector<EntryPoint> m_entry_points;
    Vector<ConstantInfo> m_constants;
    Vector<vkb::VertexInputAttributeDescription> m_vertex_attributes;
    uint32_t m_vertex_stride{0};

    explicit Shader(const Context &context) : m_context(&context) {}
    void set_module(vkb::ShaderModule module) { m_module = module; }
    void add_entry_point(String name, vkb::ShaderStage stage, Vector<spv::Id> &&interface_ids);
    void add_constant(spv::Id id, String name, uint32_t size);
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
    const Vector<EntryPoint> &entry_points() const { return m_entry_points; }
    const Vector<ConstantInfo> &constants() const { return m_constants; }
    const Vector<vkb::VertexInputAttributeDescription> &vertex_attributes() const { return m_vertex_attributes; }
    uint32_t vertex_stride() const { return m_vertex_stride; }
};

} // namespace vull::vk
