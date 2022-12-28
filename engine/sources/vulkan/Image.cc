#include <vull/vulkan/Image.hh>

#include <vull/support/Assert.hh>
#include <vull/support/Utility.hh>
#include <vull/vulkan/Allocation.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/ImageView.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull::vk {

Image::Image(Image &&other) {
    m_allocation = vull::move(other.m_allocation);
    m_full_view = vull::exchange(other.m_full_view, {});
    m_format = vull::exchange(other.m_format, {});
}

Image::~Image() {
    if (const auto *context = m_full_view.context()) {
        context->vkDestroyImage(m_full_view.image());
    }
}

Image &Image::operator=(Image &&other) {
    Image moved(vull::move(other));
    vull::swap(m_allocation, moved.m_allocation);
    vull::swap(m_full_view, moved.m_full_view);
    vull::swap(m_format, moved.m_format);
    return *this;
}

ImageView Image::create_layer_view(uint32_t layer, vkb::ImageUsage usage) {
    vkb::ImageSubresourceRange range{
        .aspectMask = m_full_view.range().aspectMask,
        .levelCount = m_full_view.range().levelCount,
        .baseArrayLayer = layer,
        .layerCount = 1,
    };
    return create_view(range, usage);
}

ImageView Image::create_level_view(uint32_t level, vkb::ImageUsage usage) {
    vkb::ImageSubresourceRange range{
        .aspectMask = m_full_view.range().aspectMask,
        .baseMipLevel = level,
        .levelCount = 1,
        .layerCount = 1,
    };
    return create_view(range, usage);
}

ImageView Image::create_view(const vkb::ImageSubresourceRange &range, vkb::ImageUsage usage) {
    vkb::ImageViewUsageCreateInfo usage_ci{
        .sType = vkb::StructureType::ImageViewUsageCreateInfo,
        .usage = usage,
    };
    vkb::ImageViewCreateInfo view_ci{
        .sType = vkb::StructureType::ImageViewCreateInfo,
        .pNext = &usage_ci,
        .image = m_full_view.image(),
        .viewType = vkb::ImageViewType::_2D,
        .format = m_format,
        .subresourceRange = range,
    };
    vkb::ImageView view;
    const auto &context = *m_full_view.context();
    VULL_ENSURE(context.vkCreateImageView(&view_ci, &view) == vkb::Result::Success);
    return {context, m_full_view.image(), view, range};
}

} // namespace vull::vk
