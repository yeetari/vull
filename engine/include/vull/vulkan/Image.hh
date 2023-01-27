#pragma once

#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/Allocation.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Context;
class Image;

class ImageView {
    friend Context;
    friend Image;

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

private:
    Allocation m_allocation;
    vkb::Format m_format{};
    vkb::Image m_image{nullptr};
    ImageView m_full_view;
    Vector<ImageView> m_views;

    Image(Allocation &&allocation, vkb::Format format, const ImageView &full_view)
        : m_allocation(vull::move(allocation)), m_format(format), m_image(full_view.image()), m_full_view(full_view) {}

public:
    Image() = default;
    Image(const Image &) = delete;
    Image(Image &&);
    ~Image();

    Image &operator=(const Image &) = delete;
    Image &operator=(Image &&);

    const ImageView &layer_view(uint32_t layer);
    const ImageView &level_view(uint32_t level);
    const ImageView &view(const vkb::ImageSubresourceRange &range);

    vkb::Image operator*() const { return m_image; }
    vkb::Format format() const { return m_format; }
    const ImageView &full_view() const { return m_full_view; }
};

} // namespace vull::vk
