#pragma once

#include <vull/vulkan/Vulkan.hh>

namespace vull::vk {

class Context;

class Pipeline {
    const Context *m_context{nullptr};
    vkb::Pipeline m_pipeline{nullptr};
    vkb::PipelineLayout m_layout{nullptr};
    vkb::PipelineBindPoint m_bind_point{};

public:
    Pipeline() = default;
    Pipeline(const Context &context, vkb::Pipeline pipeline, vkb::PipelineLayout layout,
             vkb::PipelineBindPoint bind_point)
        : m_context(&context), m_pipeline(pipeline), m_layout(layout), m_bind_point(bind_point) {}
    Pipeline(const Pipeline &) = delete;
    Pipeline(Pipeline &&);
    ~Pipeline();

    Pipeline &operator=(const Pipeline &) = delete;
    Pipeline &operator=(Pipeline &&);

    vkb::Pipeline operator*() const { return m_pipeline; }
    vkb::PipelineLayout layout() const { return m_layout; }
    vkb::PipelineBindPoint bind_point() const { return m_bind_point; }
};

} // namespace vull::vk
