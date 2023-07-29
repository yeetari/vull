#include <vull/graphics/TextureStreamer.hh>

#include <vull/container/Array.hh>
#include <vull/container/HashMap.hh>
#include <vull/container/Vector.hh>
#include <vull/core/Log.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Atomic.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Result.hh>
#include <vull/support/ScopedLock.hh>
#include <vull/support/Span.hh>
#include <vull/support/Stream.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/tasklet/Mutex.hh>
#include <vull/tasklet/Tasklet.hh>
#include <vull/vpak/FileSystem.hh>
#include <vull/vpak/PackFile.hh>
#include <vull/vpak/Reader.hh>
#include <vull/vulkan/Buffer.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/DescriptorBuilder.hh>
#include <vull/vulkan/Image.hh>
#include <vull/vulkan/MemoryUsage.hh>
#include <vull/vulkan/Queue.hh>
#include <vull/vulkan/Sampler.hh>
#include <vull/vulkan/Vulkan.hh>

#include <string.h>

namespace vull {

enum class StreamError;

namespace {

struct FormatInfo {
    vkb::Format format;
    uint32_t unit_size;
    bool block_compressed;
};

FormatInfo parse_format(uint8_t pack_format) {
    switch (vpak::ImageFormat(pack_format)) {
    case vpak::ImageFormat::Bc1Srgb:
        return {vkb::Format::Bc1RgbSrgbBlock, 8u, true};
    case vpak::ImageFormat::Bc3Srgba:
        return {vkb::Format::Bc3SrgbBlock, 16u, true};
    case vpak::ImageFormat::Bc5Unorm:
        return {vkb::Format::Bc5UnormBlock, 16u, true};
    case vpak::ImageFormat::RgUnorm:
        return {vkb::Format::R8G8Unorm, 2u, false};
    case vpak::ImageFormat::RgbaUnorm:
        return {vkb::Format::R8G8B8A8Unorm, 4u, false};
    case vpak::ImageFormat::Bc7Srgb:
        return {vkb::Format::Bc7SrgbBlock, 16u, true};
    default:
        return {vkb::Format::Undefined, 0u, false};
    }
}

// TODO: Respect options properly (sampler cache).
vk::Sampler to_sampler(vpak::ImageFilter mag_filter, vpak::ImageFilter, vpak::ImageWrapMode, vpak::ImageWrapMode) {
    if (mag_filter == vpak::ImageFilter::Linear) {
        return vk::Sampler::Linear;
    }
    return vk::Sampler::Nearest;
}

} // namespace

TextureStreamer::TextureStreamer(vk::Context &context) : m_context(context) {
    const auto set_bindings_flags = vkb::DescriptorBindingFlags::VariableDescriptorCount;
    vkb::DescriptorSetLayoutBindingFlagsCreateInfo set_binding_flags_ci{
        .sType = vkb::StructureType::DescriptorSetLayoutBindingFlagsCreateInfo,
        .bindingCount = 1,
        .pBindingFlags = &set_bindings_flags,
    };
    vkb::DescriptorSetLayoutBinding set_binding{
        .binding = 0,
        .descriptorType = vkb::DescriptorType::CombinedImageSampler,
        .descriptorCount = 32768,
        .stageFlags = vkb::ShaderStage::Fragment,
    };
    vkb::DescriptorSetLayoutCreateInfo set_layout_ci{
        .sType = vkb::StructureType::DescriptorSetLayoutCreateInfo,
        .pNext = &set_binding_flags_ci,
        .flags = vkb::DescriptorSetLayoutCreateFlags::DescriptorBufferEXT,
        .bindingCount = 1,
        .pBindings = &set_binding,
    };
    VULL_ENSURE(m_context.vkCreateDescriptorSetLayout(&set_layout_ci, &m_set_layout) == vkb::Result::Success);

    // TODO: Dynamically grow and shrink.
    // TODO: Should be in DeviceOnly memory.
    const auto buffer_size = 2048 * m_context.descriptor_size(vkb::DescriptorType::CombinedImageSampler);
    m_descriptor_buffer = m_context.create_buffer(
        buffer_size, vkb::BufferUsage::SamplerDescriptorBufferEXT | vkb::BufferUsage::TransferDst,
        vk::MemoryUsage::HostToDevice);

    constexpr Array albedo_error_colours{
        Vec<uint8_t, 4>(0xff, 0x69, 0xb4, 0xff),
        Vec<uint8_t, 4>(0x94, 0x00, 0xd3, 0xff),
    };
    Vector<Vec<uint8_t, 4>> albedo_error_data;
    for (uint32_t y = 0; y < 16; y++) {
        for (uint32_t x = 0; x < 16; x++) {
            uint32_t colour_index = (x + y) % albedo_error_colours.size();
            albedo_error_data.push(albedo_error_colours[colour_index]);
        }
    }
    auto albedo_error_image =
        create_default_image({16, 16}, vkb::Format::R8G8B8A8Unorm,
                             {reinterpret_cast<uint8_t *>(albedo_error_data.data()), albedo_error_data.size_bytes()});

    constexpr Array<uint8_t, 2> normal_error_data{
        127,
        127,
    };
    auto normal_error_image = create_default_image({1, 1}, vkb::Format::R8G8Unorm, normal_error_data.span());

    vk::DescriptorBuilder descriptor_builder(m_set_layout, m_descriptor_buffer);
    descriptor_builder.set(0, 0, albedo_error_image.full_view().sampled(vk::Sampler::Nearest));
    descriptor_builder.set(0, 1, normal_error_image.full_view().sampled(vk::Sampler::Linear));

    // Transfer ownership of images.
    m_images.push(vull::move(albedo_error_image));
    m_images.push(vull::move(normal_error_image));
}

TextureStreamer::~TextureStreamer() {
    m_context.vkDestroyDescriptorSetLayout(m_set_layout);

    // Wait for any in progress uploads to complete.
    // TODO: A more sophisticated way of doing this.
    while (m_in_progress.load() != 0) {
    }
}

vk::Image TextureStreamer::create_default_image(Vec2u extent, vkb::Format format, Span<const uint8_t> pixel_data) {
    vkb::ImageCreateInfo image_ci{
        .sType = vkb::StructureType::ImageCreateInfo,
        .imageType = vkb::ImageType::_2D,
        .format = format,
        .extent = {extent.x(), extent.y(), 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vkb::SampleCount::_1,
        .tiling = vkb::ImageTiling::Optimal,
        .usage = vkb::ImageUsage::TransferDst | vkb::ImageUsage::Sampled,
        .sharingMode = vkb::SharingMode::Exclusive,
        .initialLayout = vkb::ImageLayout::Undefined,
    };
    auto image = m_context.create_image(image_ci, vk::MemoryUsage::DeviceOnly);

    auto staging_buffer =
        m_context.create_buffer(pixel_data.size(), vkb::BufferUsage::TransferSrc, vk::MemoryUsage::HostOnly);
    memcpy(staging_buffer.mapped_raw(), pixel_data.data(), pixel_data.size());

    // Perform CPU -> GPU copy.
    m_context.graphics_queue().immediate_submit([&](const vk::CommandBuffer &cmd_buf) {
        vkb::ImageMemoryBarrier2 transfer_write_barrier{
            .sType = vkb::StructureType::ImageMemoryBarrier2,
            .dstStageMask = vkb::PipelineStage2::Copy,
            .dstAccessMask = vkb::Access2::TransferWrite,
            .oldLayout = vkb::ImageLayout::Undefined,
            .newLayout = vkb::ImageLayout::TransferDstOptimal,
            .image = *image,
            .subresourceRange = image.full_view().range(),
        };
        cmd_buf.image_barrier(transfer_write_barrier);

        vkb::BufferImageCopy copy{
            .imageSubresource{
                .aspectMask = vkb::ImageAspect::Color,
                .layerCount = 1,
            },
            .imageExtent = {extent.x(), extent.y(), 1},
        };
        cmd_buf.copy_buffer_to_image(staging_buffer, image, vkb::ImageLayout::TransferDstOptimal, copy);

        vkb::ImageMemoryBarrier2 image_read_barrier{
            .sType = vkb::StructureType::ImageMemoryBarrier2,
            .srcStageMask = vkb::PipelineStage2::Copy,
            .srcAccessMask = vkb::Access2::TransferWrite,
            .dstStageMask = vkb::PipelineStage2::AllCommands,
            .dstAccessMask = vkb::Access2::ShaderSampledRead,
            .oldLayout = vkb::ImageLayout::TransferDstOptimal,
            .newLayout = vkb::ImageLayout::ReadOnlyOptimal,
            .image = *image,
            .subresourceRange = image.full_view().range(),
        };
        cmd_buf.image_barrier(image_read_barrier);
    });
    return image;
}

Result<uint32_t, StreamError> TextureStreamer::load_texture(Stream &stream) {
    const auto [format, unit_size, block_compressed] = parse_format(VULL_TRY(stream.read_byte()));
    const auto mag_filter = static_cast<vpak::ImageFilter>(VULL_TRY(stream.read_byte()));
    const auto min_filter = static_cast<vpak::ImageFilter>(VULL_TRY(stream.read_byte()));
    const auto wrap_u = static_cast<vpak::ImageWrapMode>(VULL_TRY(stream.read_byte()));
    const auto wrap_v = static_cast<vpak::ImageWrapMode>(VULL_TRY(stream.read_byte()));
    const auto width = VULL_TRY(stream.read_varint<uint32_t>());
    const auto height = VULL_TRY(stream.read_varint<uint32_t>());
    const auto mip_count = VULL_TRY(stream.read_varint<uint32_t>());

    vkb::ImageCreateInfo image_ci{
        .sType = vkb::StructureType::ImageCreateInfo,
        .imageType = vkb::ImageType::_2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = mip_count,
        .arrayLayers = 1,
        .samples = vkb::SampleCount::_1,
        .tiling = vkb::ImageTiling::Optimal,
        .usage = vkb::ImageUsage::TransferDst | vkb::ImageUsage::Sampled,
        .sharingMode = vkb::SharingMode::Exclusive,
        .initialLayout = vkb::ImageLayout::Undefined,
    };
    auto image = m_context.create_image(image_ci, vk::MemoryUsage::DeviceOnly);

    // TODO: Not correct at all!
    vk::Queue queue(m_context, 0);

    // Transition the whole image (all mip levels) to TransferDstOptimal.
    queue.immediate_submit([&image, mip_count](const vk::CommandBuffer &cmd_buf) {
        vkb::ImageMemoryBarrier2 transfer_write_barrier{
            .sType = vkb::StructureType::ImageMemoryBarrier2,
            .dstStageMask = vkb::PipelineStage2::Copy,
            .dstAccessMask = vkb::Access2::TransferWrite,
            .oldLayout = vkb::ImageLayout::Undefined,
            .newLayout = vkb::ImageLayout::TransferDstOptimal,
            .image = *image,
            .subresourceRange{
                .aspectMask = vkb::ImageAspect::Color,
                .levelCount = mip_count,
                .layerCount = 1,
            },
        };
        cmd_buf.image_barrier(transfer_write_barrier);
    });

    uint32_t mip_width = width;
    uint32_t mip_height = height;
    for (uint32_t i = 0; i < mip_count; i++) {
        const uint32_t mip_size = block_compressed ? ((mip_width + 3) / 4) * ((mip_height + 3) / 4) * unit_size
                                                   : mip_width * mip_height * unit_size;
        auto staging_buffer =
            m_context.create_buffer(mip_size, vkb::BufferUsage::TransferSrc, vk::MemoryUsage::HostOnly);
        VULL_TRY(stream.read({staging_buffer.mapped_raw(), mip_size}));

        // Perform CPU -> GPU copy.
        queue.immediate_submit([&](const vk::CommandBuffer &cmd_buf) {
            vkb::BufferImageCopy copy{
                .imageSubresource{
                    .aspectMask = vkb::ImageAspect::Color,
                    .mipLevel = i,
                    .layerCount = 1,
                },
                .imageExtent = {mip_width, mip_height, 1},
            };
            cmd_buf.copy_buffer_to_image(staging_buffer, image, vkb::ImageLayout::TransferDstOptimal, copy);
        });
        mip_width >>= 1;
        mip_height >>= 1;
    }

    // Transition the whole image to ReadOnlyOptimal.
    queue.immediate_submit([&image, mip_count](const vk::CommandBuffer &cmd_buf) {
        vkb::ImageMemoryBarrier2 image_read_barrier{
            .sType = vkb::StructureType::ImageMemoryBarrier2,
            .srcStageMask = vkb::PipelineStage2::Copy,
            .srcAccessMask = vkb::Access2::TransferWrite,
            .dstStageMask = vkb::PipelineStage2::AllCommands,
            .dstAccessMask = vkb::Access2::ShaderRead,
            .oldLayout = vkb::ImageLayout::TransferDstOptimal,
            .newLayout = vkb::ImageLayout::ReadOnlyOptimal,
            .image = *image,
            .subresourceRange{
                .aspectMask = vkb::ImageAspect::Color,
                .levelCount = mip_count,
                .layerCount = 1,
            },
        };
        cmd_buf.image_barrier(image_read_barrier);
    });

    ScopedLock images_lock(m_images_mutex);
    const auto next_index = m_images.size();
    auto sampled_image = image.full_view().sampled(to_sampler(mag_filter, min_filter, wrap_u, wrap_v));
    m_images.push(vull::move(image));
    vk::DescriptorBuilder descriptor_builder(m_set_layout, m_descriptor_buffer);
    descriptor_builder.set(0, next_index, sampled_image);
    return next_index;
}

void TextureStreamer::load_texture(String &&name) {
    auto stream = vpak::open(name);
    if (!stream) {
        vull::error("[graphics] Failed to find texture {}", name);
        ScopedLock lock(m_mutex);
        m_texture_indices.set(vull::move(name), 0);
        return;
    }

    if (auto index = load_texture(*stream).to_optional()) {
        ScopedLock lock(m_mutex);
        m_texture_indices.set(vull::move(name), *index);
    } else {
        vull::error("[graphics] Failed to load texture {}", name);
        ScopedLock lock(m_mutex);
        m_texture_indices.set(vull::move(name), 0);
    }
}

uint32_t TextureStreamer::ensure_texture(StringView name, TextureKind kind) {
    const uint32_t fallback_index = kind == TextureKind::Normal ? 1 : 0;

    ScopedLock lock(m_mutex);
    if (auto index = m_texture_indices.get(name)) {
        return *index != UINT32_MAX ? *index : fallback_index;
    }

    bool scheduled = vull::try_schedule([this, name = String(name)]() mutable {
        load_texture(vull::move(name));
        m_in_progress.fetch_sub(1);
    });
    if (scheduled) {
        m_in_progress.fetch_add(1);
        m_texture_indices.set(name, UINT32_MAX);
    }
    return fallback_index;
}

} // namespace vull
