#pragma once

#include <vull/support/Utility.hh>
#include <vull/vulkan/Allocation.hh>
#include <vull/vulkan/ImageView.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Context;

class Image {
    friend Context;

private:
    Allocation m_allocation;
    ImageView m_full_view;
    vkb::Format m_format{};

    Image(Allocation &&allocation, ImageView &&full_view, vkb::Format format)
        : m_allocation(vull::move(allocation)), m_full_view(vull::move(full_view)), m_format(format) {}

public:
    Image() = default;
    Image(const Image &) = delete;
    Image(Image &&);
    ~Image();

    Image &operator=(const Image &) = delete;
    Image &operator=(Image &&);

    // TODO: Create and cache these on image creation?
    ImageView create_layer_view(uint32_t layer, vkb::ImageUsage usage);
    ImageView create_level_view(uint32_t level, vkb::ImageUsage usage);
    ImageView create_view(const vkb::ImageSubresourceRange &range, vkb::ImageUsage usage);

    vkb::Image operator*() const { return m_full_view.image(); }
    vkb::Format format() const { return m_format; }
    const ImageView &full_view() const { return m_full_view; }
};

} // namespace vull::vk
