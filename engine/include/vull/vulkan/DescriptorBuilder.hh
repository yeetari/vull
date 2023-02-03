#pragma once

#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Buffer;
class Context;
class ImageView;

class DescriptorBuilder {
    const Context *m_context{nullptr};
    vkb::DescriptorSetLayout m_layout{nullptr};
    uint8_t *m_data{nullptr};

public:
    DescriptorBuilder() = default;
    DescriptorBuilder(const Context &context, vkb::DescriptorSetLayout layout, uint8_t *data)
        : m_context(&context), m_layout(layout), m_data(data) {}
    DescriptorBuilder(vkb::DescriptorSetLayout layout, const Buffer &buffer);

    void set(uint32_t binding, vkb::Sampler sampler);
    void set(uint32_t binding, uint32_t element, vkb::Sampler sampler, const ImageView &view);
    void set(uint32_t binding, const ImageView &view, bool storage);
    void set(uint32_t binding, const Buffer &buffer);
};

} // namespace vull::vk
