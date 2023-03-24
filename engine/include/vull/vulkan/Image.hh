#pragma once

#include <vull/support/Vector.hh>
#include <vull/vulkan/Allocation.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Context;
class Image;
class Swapchain;

class ImageView {
    friend Context;
    friend Image;
    friend Swapchain;

private:
    vkb::Image m_image;
    vkb::ImageView m_view;
    vkb::ImageSubresourceRange m_range;

    ImageView() = default;
    ImageView(vkb::Image image, vkb::ImageView view, const vkb::ImageSubresourceRange &range)
        : m_image(image), m_view(view), m_range(range) {}

public:
    vkb::ImageView operator*() const { return m_view; }
    vkb::Image image() const { return m_image; }
    const vkb::ImageSubresourceRange &range() const { return m_range; }
};

class Image {
    friend Context;
    friend Swapchain;

private:
    const Context *m_context{nullptr};
    Allocation m_allocation{};
    vkb::Format m_format{};
    vkb::Image m_owned_image{nullptr};
    ImageView m_full_view;
    mutable Vector<ImageView> m_views;

    Image(Allocation &&allocation, vkb::Format format, const ImageView &full_view);
    Image(const Context &context, vkb::Format format, const ImageView &full_view)
        : m_context(&context), m_format(format), m_full_view(full_view) {}

public:
    Image() = default;
    Image(const Image &) = delete;
    Image(Image &&);
    ~Image();

    Image &operator=(const Image &) = delete;
    Image &operator=(Image &&);

    const ImageView &layer_view(uint32_t layer) const;
    const ImageView &level_view(uint32_t level) const;
    const ImageView &swizzle_view(const vkb::ComponentMapping &mapping) const;
    const ImageView &view(const vkb::ImageSubresourceRange &range, const vkb::ComponentMapping &mapping) const;

    vkb::Image operator*() const { return m_full_view.image(); }
    vkb::Format format() const { return m_format; }
    const ImageView &full_view() const { return m_full_view; }
};

} // namespace vull::vk
