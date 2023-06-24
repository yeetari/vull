#pragma once

#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Buffer;
class Context;
class ImageView;
class SampledImage;
enum class Sampler;

class DescriptorBuilder {
    const Context *m_context{nullptr};
    vkb::DescriptorSetLayout m_layout{nullptr};
    uint8_t *m_data{nullptr};

public:
    DescriptorBuilder() = default;
    DescriptorBuilder(const Context &context, vkb::DescriptorSetLayout layout, uint8_t *data)
        : m_context(&context), m_layout(layout), m_data(data) {}
    DescriptorBuilder(vkb::DescriptorSetLayout layout, const Buffer &buffer);

    void set(uint32_t binding, uint32_t element, vk::Sampler sampler);
    void set(uint32_t binding, uint32_t element, const Buffer &buffer);
    void set(uint32_t binding, uint32_t element, const SampledImage &image);
    void set(uint32_t binding, uint32_t element, const ImageView &view);
};

} // namespace vull::vk
