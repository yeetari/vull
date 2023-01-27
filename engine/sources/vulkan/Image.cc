#include <vull/vulkan/Image.hh>

#include <vull/support/Assert.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/Allocation.hh>
#include <vull/vulkan/Allocator.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Vulkan.hh>

#include <string.h>

namespace vull::vk {

Image::Image(Image &&other) {
    m_allocation = vull::move(other.m_allocation);
    m_format = vull::exchange(other.m_format, {});
    m_image = vull::exchange(other.m_image, nullptr);
    m_full_view = vull::exchange(other.m_full_view, {});
    m_views = vull::exchange(other.m_views, {});
}

Image::~Image() {
    if (const auto *allocator = m_allocation.allocator()) {
        const auto &context = allocator->context();
        for (const auto &view : m_views) {
            context.vkDestroyImageView(*view);
        }
        context.vkDestroyImageView(*m_full_view);
        context.vkDestroyImage(m_image);
    }
}

Image &Image::operator=(Image &&other) {
    Image moved(vull::move(other));
    vull::swap(m_allocation, moved.m_allocation);
    vull::swap(m_format, moved.m_format);
    vull::swap(m_image, moved.m_image);
    vull::swap(m_full_view, moved.m_full_view);
    vull::swap(m_views, moved.m_views);
    return *this;
}

const ImageView &Image::layer_view(uint32_t layer) {
    vkb::ImageSubresourceRange range{
        .aspectMask = m_full_view.range().aspectMask,
        .levelCount = m_full_view.range().levelCount,
        .baseArrayLayer = layer,
        .layerCount = 1,
    };
    return view(range);
}

const ImageView &Image::level_view(uint32_t level) {
    vkb::ImageSubresourceRange range{
        .aspectMask = m_full_view.range().aspectMask,
        .baseMipLevel = level,
        .levelCount = 1,
        .layerCount = 1,
    };
    return view(range);
}

const ImageView &Image::view(const vkb::ImageSubresourceRange &range) {
    for (const auto &view : m_views) {
        if (memcmp(&range, &view.range(), sizeof(vkb::ImageSubresourceRange)) == 0) {
            return view;
        }
    }
    vkb::ImageViewCreateInfo view_ci{
        .sType = vkb::StructureType::ImageViewCreateInfo,
        .image = m_image,
        .viewType = vkb::ImageViewType::_2D,
        .format = m_format,
        .subresourceRange = range,
    };
    const auto &context = m_allocation.allocator()->context();
    vkb::ImageView view;
    VULL_ENSURE(context.vkCreateImageView(&view_ci, &view) == vkb::Result::Success);
    return m_views.emplace(ImageView(m_image, view, range));
}

} // namespace vull::vk
