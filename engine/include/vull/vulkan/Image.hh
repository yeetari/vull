#pragma once

#include <vull/support/Vector.hh>
#include <vull/vulkan/Allocation.hh>
#include <vull/vulkan/Sampler.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Context;
class Image;
class SampledImage;
class Swapchain;

class ImageView {
    friend Context;
    friend Image;
    friend Swapchain;

private:
    const Context *m_context{nullptr};
    vkb::Image m_image;
    vkb::ImageView m_view;
    vkb::ImageSubresourceRange m_range;

    ImageView() = default;
    ImageView(const Context *context, vkb::Image image, vkb::ImageView view, const vkb::ImageSubresourceRange &range)
        : m_context(context), m_image(image), m_view(view), m_range(range) {}

public:
    SampledImage sampled(Sampler sampler) const;

    vkb::ImageView operator*() const { return m_view; }
    vkb::Image image() const { return m_image; }
    const vkb::ImageSubresourceRange &range() const { return m_range; }
};

class SampledImage {
    friend ImageView;

private:
    ImageView m_view;
    vkb::Sampler m_sampler;

    SampledImage(ImageView view, vkb::Sampler sampler) : m_view(view), m_sampler(sampler) {}

public:
    const ImageView &view() const { return m_view; }
    vkb::Sampler sampler() const { return m_sampler; }
};

class Image {
    friend Context;
    friend Swapchain;

private:
    const Context *m_context{nullptr};
    Allocation m_allocation{};
    vkb::Extent3D m_extent{};
    vkb::Format m_format{};
    vkb::Image m_owned_image{nullptr};
    ImageView m_full_view;
    mutable Vector<ImageView> m_views;

    Image(Allocation &&allocation, vkb::Extent3D extent, vkb::Format format, const ImageView &full_view);
    Image(const Context &context, vkb::Extent3D extent, vkb::Format format, const ImageView &full_view)
        : m_context(&context), m_extent(extent), m_format(format), m_full_view(full_view) {}

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
    vkb::Extent3D extent() const { return m_extent; }
    vkb::Format format() const { return m_format; }
    const ImageView &full_view() const { return m_full_view; }
};

} // namespace vull::vk
