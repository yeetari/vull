#include <vull/vulkan/DescriptorBuilder.hh>

#include <vull/support/Assert.hh>
#include <vull/support/Utility.hh>
#include <vull/vulkan/Buffer.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Image.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull::vk {

enum class Sampler;

DescriptorBuilder::DescriptorBuilder(vkb::DescriptorSetLayout layout, const Buffer &buffer)
    : m_context(&buffer.context()), m_layout(layout), m_data(buffer.mapped<uint8_t>()) {}

void DescriptorBuilder::set(uint32_t binding, uint32_t element, vk::Sampler sampler) {
    vkb::DeviceSize offset;
    m_context->vkGetDescriptorSetLayoutBindingOffsetEXT(m_layout, binding, &offset);

    vkb::Sampler vk_sampler = m_context->get_sampler(sampler);
    const auto size = m_context->descriptor_size(vkb::DescriptorType::Sampler);
    offset += element * size;
    vkb::DescriptorGetInfoEXT get_info{
        .sType = vkb::StructureType::DescriptorGetInfoEXT,
        .type = vkb::DescriptorType::Sampler,
        .data{
            .pSampler = &vk_sampler,
        },
    };
    m_context->vkGetDescriptorEXT(&get_info, size, m_data + offset);
}

void DescriptorBuilder::set(uint32_t binding, uint32_t element, const Buffer &buffer) {
    vkb::DeviceSize offset;
    m_context->vkGetDescriptorSetLayoutBindingOffsetEXT(m_layout, binding, &offset);

    const bool is_storage = (buffer.usage() & vkb::BufferUsage::StorageBuffer) == vkb::BufferUsage::StorageBuffer;
    const bool is_uniform = (buffer.usage() & vkb::BufferUsage::UniformBuffer) == vkb::BufferUsage::UniformBuffer;
    VULL_ASSERT(is_storage ^ is_uniform);
    VULL_IGNORE(is_uniform);

    const auto type = is_storage ? vkb::DescriptorType::StorageBuffer : vkb::DescriptorType::UniformBuffer;
    const auto size = m_context->descriptor_size(type);
    offset += element * size;
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
    m_context->vkGetDescriptorEXT(&get_info, size, m_data + offset);
}

void DescriptorBuilder::set(uint32_t binding, uint32_t element, const SampledImage &image) {
    vkb::DeviceSize offset;
    m_context->vkGetDescriptorSetLayoutBindingOffsetEXT(m_layout, binding, &offset);

    const bool has_sampler = image.sampler() != nullptr;
    const auto type = has_sampler ? vkb::DescriptorType::CombinedImageSampler : vkb::DescriptorType::SampledImage;
    const auto size = m_context->descriptor_size(type);
    offset += element * size;
    vkb::DescriptorImageInfo image_info{
        .sampler = image.sampler(),
        .imageView = *image.view(),
        .imageLayout = vkb::ImageLayout::ReadOnlyOptimal,
    };
    vkb::DescriptorGetInfoEXT get_info{
        .sType = vkb::StructureType::DescriptorGetInfoEXT,
        .type = type,
        .data{
            .pCombinedImageSampler = &image_info,
        },
    };
    m_context->vkGetDescriptorEXT(&get_info, size, m_data + offset);
}

void DescriptorBuilder::set(uint32_t binding, uint32_t element, const ImageView &view) {
    vkb::DeviceSize offset;
    m_context->vkGetDescriptorSetLayoutBindingOffsetEXT(m_layout, binding, &offset);

    const auto size = m_context->descriptor_size(vkb::DescriptorType::StorageImage);
    offset += element * size;
    vkb::DescriptorImageInfo image_info{
        .imageView = *view,
        .imageLayout = vkb::ImageLayout::General,
    };
    vkb::DescriptorGetInfoEXT get_info{
        .sType = vkb::StructureType::DescriptorGetInfoEXT,
        .type = vkb::DescriptorType::StorageImage,
        .data{
            .pStorageImage = &image_info,
        },
    };
    m_context->vkGetDescriptorEXT(&get_info, size, m_data + offset);
}

} // namespace vull::vk
