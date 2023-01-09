#pragma once

#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Buffer;
class Context;
class ImageView;

class DescriptorBuilder {
    const Context &m_context;
    uint8_t *m_ptr{nullptr};

public:
    DescriptorBuilder(const Context &context, uint8_t *ptr) : m_context(context), m_ptr(ptr) {}
    DescriptorBuilder(const Buffer &buffer);
    DescriptorBuilder(const DescriptorBuilder &) = delete;
    DescriptorBuilder(DescriptorBuilder &&) = delete;
    ~DescriptorBuilder() = default;

    DescriptorBuilder &operator=(const DescriptorBuilder &) = delete;
    DescriptorBuilder &operator=(DescriptorBuilder &&) = delete;

    void put(vkb::Sampler sampler);
    void put(vkb::Sampler sampler, const ImageView &view);
    void put(const ImageView &view, bool storage);
    void put(const Buffer &buffer);

    // TODO: Only here because swapchain image views aren't (yet) wrapped in ImageView.
    void put(vkb::ImageView view);
};

} // namespace vull::vk
