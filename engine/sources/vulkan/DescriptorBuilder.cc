#include <vull/vulkan/DescriptorBuilder.hh>

#include <vull/support/Assert.hh>
#include <vull/support/Utility.hh>
#include <vull/vulkan/Buffer.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/ImageView.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull::vk {

DescriptorBuilder::DescriptorBuilder(const Buffer &buffer)
    : m_context(buffer.context()), m_ptr(buffer.mapped<uint8_t>()) {}

void DescriptorBuilder::put(vkb::Sampler sampler) {
    const auto size = m_context.descriptor_size(vkb::DescriptorType::Sampler);
    vkb::DescriptorGetInfoEXT get_info{
        .sType = vkb::StructureType::DescriptorGetInfoEXT,
        .type = vkb::DescriptorType::Sampler,
        .data{
            .pSampler = &sampler,
        },
    };
    m_context.vkGetDescriptorEXT(&get_info, size, m_ptr);
    m_ptr += size;
}

void DescriptorBuilder::put(vkb::Sampler sampler, const ImageView &view) {
    const auto size = m_context.descriptor_size(vkb::DescriptorType::CombinedImageSampler);
    vkb::DescriptorImageInfo image_info{
        .sampler = sampler,
        .imageView = *view,
        .imageLayout = vkb::ImageLayout::ReadOnlyOptimal,
    };
    vkb::DescriptorGetInfoEXT get_info{
        .sType = vkb::StructureType::DescriptorGetInfoEXT,
        .type = vkb::DescriptorType::CombinedImageSampler,
        .data{
            .pCombinedImageSampler = &image_info,
        },
    };
    m_context.vkGetDescriptorEXT(&get_info, size, m_ptr);
    m_ptr += size;
}

void DescriptorBuilder::put(const ImageView &view, bool storage) {
    const auto type = storage ? vkb::DescriptorType::StorageImage : vkb::DescriptorType::SampledImage;
    const auto size = m_context.descriptor_size(type);
    vkb::DescriptorImageInfo image_info{
        .imageView = *view,
        .imageLayout = storage ? vkb::ImageLayout::General : vkb::ImageLayout::ReadOnlyOptimal,
    };
    vkb::DescriptorGetInfoEXT get_info{
        .sType = vkb::StructureType::DescriptorGetInfoEXT,
        .type = type,
        .data{
            .pSampledImage = &image_info,
        },
    };
    m_context.vkGetDescriptorEXT(&get_info, size, m_ptr);
    m_ptr += size;
}

void DescriptorBuilder::put(const Buffer &buffer) {
    const bool is_storage = (buffer.usage() & vkb::BufferUsage::StorageBuffer) == vkb::BufferUsage::StorageBuffer;
    const bool is_uniform = (buffer.usage() & vkb::BufferUsage::UniformBuffer) == vkb::BufferUsage::UniformBuffer;
    VULL_ASSERT(is_storage ^ is_uniform);
    VULL_IGNORE(is_uniform);

    const auto type = is_storage ? vkb::DescriptorType::StorageBuffer : vkb::DescriptorType::UniformBuffer;
    const auto size = m_context.descriptor_size(type);
    vkb::DescriptorAddressInfoEXT address_info{
        .sType = vkb::StructureType::DescriptorAddressInfoEXT,
        .address = buffer.device_address(),
        .range = buffer.size(),
    };
    vkb::DescriptorGetInfoEXT get_info{
        .sType = vkb::StructureType::DescriptorGetInfoEXT,
        .type = type,
        .data{
            .pStorageBuffer = &address_info,
        },
    };
    m_context.vkGetDescriptorEXT(&get_info, size, m_ptr);
    m_ptr += size;
}

void DescriptorBuilder::put(vkb::ImageView view) {
    const auto size = m_context.descriptor_size(vkb::DescriptorType::StorageImage);
    vkb::DescriptorImageInfo image_info{
        .imageView = view,
        .imageLayout = vkb::ImageLayout::General,
    };
    vkb::DescriptorGetInfoEXT get_info{
        .sType = vkb::StructureType::DescriptorGetInfoEXT,
        .type = vkb::DescriptorType::StorageImage,
        .data{
            .pSampledImage = &image_info,
        },
    };
    m_context.vkGetDescriptorEXT(&get_info, size, m_ptr);
    m_ptr += size;
}

} // namespace vull::vk
