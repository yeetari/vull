#pragma once

#include <vull/container/hash_map.hh>
#include <vull/container/vector.hh>
#include <vull/support/optional.hh>
#include <vull/support/result.hh>
#include <vull/support/string.hh>
#include <vull/vulkan/pipeline.hh>
#include <vull/vulkan/vulkan.hh>

namespace vull::vk {

class Context;
class Shader;

struct UnspecifiedConstantError {
    StringView name;
};

using PipelineResult = Result<Pipeline, vkb::Result, UnspecifiedConstantError>;

class PipelineBuilder {
    HashMap<String, size_t> m_constants;
    Vector<vkb::PipelineColorBlendAttachmentState> m_blend_states;
    Vector<vkb::Format> m_colour_formats;
    Vector<vkb::DescriptorSetLayout> m_set_layouts;
    Vector<const Shader &> m_shaders;
    vkb::CullMode m_cull_mode{vkb::CullMode::None};
    float m_depth_bias_cf{0.0f};
    float m_depth_bias_sf{0.0f};
    vkb::Format m_depth_format{vkb::Format::Undefined};
    vkb::CompareOp m_depth_op{vkb::CompareOp::Always};
    vkb::FrontFace m_front_face{};
    vkb::PolygonMode m_polygon_mode{vkb::PolygonMode::Fill};
    vkb::PushConstantRange m_push_constant_range{};
    vkb::PrimitiveTopology m_topology{};
    bool m_depth_test_enabled{false};
    bool m_depth_write_enabled{false};

public:
    PipelineBuilder &add_colour_attachment(vkb::Format format,
                                           Optional<const vkb::PipelineColorBlendAttachmentState &> blend_state = {});
    PipelineBuilder &add_set_layout(vkb::DescriptorSetLayout set_layout);
    PipelineBuilder &add_shader(const Shader &shader);
    template <typename T>
    PipelineBuilder &set_constant(String name, T value);
    PipelineBuilder &set_cull_mode(vkb::CullMode cull_mode, vkb::FrontFace front_face);
    PipelineBuilder &set_depth_bias(float cf, float sf);
    PipelineBuilder &set_depth_format(vkb::Format format);
    PipelineBuilder &set_depth_params(vkb::CompareOp op, bool test_enabled, bool write_enabled);
    PipelineBuilder &set_polygon_mode(vkb::PolygonMode polygon_mode);
    PipelineBuilder &set_push_constant_range(const vkb::PushConstantRange &push_constant_range);
    PipelineBuilder &set_topology(vkb::PrimitiveTopology topology);
    PipelineResult build(const Context &context);
};

} // namespace vull::vk
