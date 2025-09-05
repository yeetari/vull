#include <vull/vulkan/image.hh>

#include <vull/container/vector.hh>
#include <vull/support/assert.hh>
#include <vull/support/utility.hh>
#include <vull/vulkan/allocation.hh>
#include <vull/vulkan/allocator.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/vulkan.hh>

#include <string.h>

namespace vull::vk {

enum class Sampler;

Image::Image(DeviceMemoryAllocation &&allocation, vkb::Extent3D extent, vkb::Format format, const ImageView &full_view)
    : m_context(&allocation.heap().context()), m_allocation(vull::move(allocation)), m_extent(extent), m_format(format),
      m_owned_image(full_view.image()), m_full_view(full_view) {}

Image::Image(Image &&other) {
    m_context = vull::exchange(other.m_context, nullptr);
    m_allocation = vull::move(other.m_allocation);
    m_extent = vull::move(other.m_extent);
    m_format = vull::exchange(other.m_format, {});
    m_owned_image = vull::exchange(other.m_owned_image, nullptr);
    m_full_view = vull::exchange(other.m_full_view, {});
    m_views = vull::exchange(other.m_views, {});
}

Image::~Image() {
    if (m_context != nullptr) {
        for (const auto &view : m_views) {
            m_context->vkDestroyImageView(*view);
        }
        m_context->vkDestroyImageView(*m_full_view);
        m_context->vkDestroyImage(m_owned_image);
    }
}

Image &Image::operator=(Image &&other) {
    Image moved(vull::move(other));
    vull::swap(m_context, moved.m_context);
    vull::swap(m_allocation, moved.m_allocation);
    vull::swap(m_extent, moved.m_extent);
    vull::swap(m_format, moved.m_format);
    vull::swap(m_owned_image, moved.m_owned_image);
    vull::swap(m_full_view, moved.m_full_view);
    vull::swap(m_views, moved.m_views);
    return *this;
}

const ImageView &Image::layer_view(uint32_t layer) const {
    vkb::ImageSubresourceRange range{
        .aspectMask = m_full_view.range().aspectMask,
        .levelCount = m_full_view.range().levelCount,
        .baseArrayLayer = layer,
        .layerCount = 1,
    };
    return view(range, {});
}

const ImageView &Image::level_view(uint32_t level) const {
    vkb::ImageSubresourceRange range{
        .aspectMask = m_full_view.range().aspectMask,
        .baseMipLevel = level,
        .levelCount = 1,
        .layerCount = 1,
    };
    return view(range, {});
}

const ImageView &Image::swizzle_view(const vkb::ComponentMapping &mapping) const {
    vkb::ImageSubresourceRange range{
        .aspectMask = m_full_view.range().aspectMask,
        .levelCount = m_full_view.range().levelCount,
        .layerCount = 1,
    };
    return view(range, mapping);
}

const ImageView &Image::view(const vkb::ImageSubresourceRange &range, const vkb::ComponentMapping &mapping) const {
    for (const auto &view : m_views) {
        if (memcmp(&range, &view.range(), sizeof(vkb::ImageSubresourceRange)) == 0) {
            return view;
        }
    }
    vkb::ImageViewCreateInfo view_ci{
        .sType = vkb::StructureType::ImageViewCreateInfo,
        .image = m_full_view.image(),
        .viewType = vkb::ImageViewType::_2D,
        .format = m_format,
        .components = mapping,
        .subresourceRange = range,
    };
    vkb::ImageView view;
    VULL_ENSURE(m_context->vkCreateImageView(&view_ci, &view) == vkb::Result::Success);
    return m_views.emplace(ImageView(m_context, m_full_view.image(), view, range));
}

SampledImage ImageView::sampled(Sampler sampler) const {
    return {*this, m_context->get_sampler(sampler)};
}

} // namespace vull::vk
