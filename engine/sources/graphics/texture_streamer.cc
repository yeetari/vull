#include <vull/graphics/texture_streamer.hh>

#include <vull/container/array.hh>
#include <vull/container/hash_map.hh>
#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/maths/vec.hh>
#include <vull/support/assert.hh>
#include <vull/support/optional.hh>
#include <vull/support/result.hh>
#include <vull/support/scoped_lock.hh>
#include <vull/support/span.hh>
#include <vull/support/stream.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/functions.hh>
#include <vull/tasklet/future.hh>
#include <vull/tasklet/mutex.hh>
#include <vull/vpak/defs.hh>
#include <vull/vpak/file_system.hh>
#include <vull/vpak/stream.hh>
#include <vull/vulkan/buffer.hh>
#include <vull/vulkan/command_buffer.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/descriptor_builder.hh>
#include <vull/vulkan/image.hh>
#include <vull/vulkan/memory_usage.hh>
#include <vull/vulkan/queue.hh>
#include <vull/vulkan/sampler.hh>
#include <vull/vulkan/vulkan.hh>

#include <string.h>

namespace vull {

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
    for (auto &[_, future] : m_futures) {
        future.await();
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
    auto &queue = m_context.get_queue(vk::QueueKind::Transfer);
    auto cmd_buf = queue.request_cmd_buf();
    vkb::ImageMemoryBarrier2 transfer_write_barrier{
        .sType = vkb::StructureType::ImageMemoryBarrier2,
        .dstStageMask = vkb::PipelineStage2::Copy,
        .dstAccessMask = vkb::Access2::TransferWrite,
        .oldLayout = vkb::ImageLayout::Undefined,
        .newLayout = vkb::ImageLayout::TransferDstOptimal,
        .image = *image,
        .subresourceRange = image.full_view().range(),
    };
    cmd_buf->image_barrier(transfer_write_barrier);

    vkb::BufferImageCopy copy{
        .imageSubresource{
            .aspectMask = vkb::ImageAspect::Color,
            .layerCount = 1,
        },
        .imageExtent = {extent.x(), extent.y(), 1},
    };
    cmd_buf->copy_buffer_to_image(staging_buffer, image, vkb::ImageLayout::TransferDstOptimal, copy);

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
    cmd_buf->image_barrier(image_read_barrier);
    queue.submit(vull::move(cmd_buf), {}, {}).await();
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

    auto &queue = m_context.get_queue(vk::QueueKind::Transfer);
    auto cmd_buf = queue.request_cmd_buf();

    // Transition the whole image (all mip levels) to TransferDstOptimal.
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
    cmd_buf->image_barrier(transfer_write_barrier);

    uint32_t mip_width = width;
    uint32_t mip_height = height;
    for (uint32_t i = 0; i < mip_count; i++) {
        const uint32_t mip_size = block_compressed ? ((mip_width + 3) / 4) * ((mip_height + 3) / 4) * unit_size
                                                   : mip_width * mip_height * unit_size;
        auto staging_buffer =
            m_context.create_buffer(mip_size, vkb::BufferUsage::TransferSrc, vk::MemoryUsage::HostOnly);
        VULL_TRY(stream.read({staging_buffer.mapped_raw(), mip_size}));

        // Queue CPU -> GPU copy and transfer ownership of staging buffer so that it can be freed upon command buffer
        // completion.
        vkb::BufferImageCopy copy{
            .imageSubresource{
                .aspectMask = vkb::ImageAspect::Color,
                .mipLevel = i,
                .layerCount = 1,
            },
            .imageExtent = {mip_width, mip_height, 1},
        };
        cmd_buf->copy_buffer_to_image(staging_buffer, image, vkb::ImageLayout::TransferDstOptimal, copy);
        cmd_buf->bind_associated_buffer(vull::move(staging_buffer));

        mip_width >>= 1;
        mip_height >>= 1;
    }

    // Transition the whole image to ReadOnlyOptimal.
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
    cmd_buf->image_barrier(image_read_barrier);

    // Submit command buffer.
    queue.submit(vull::move(cmd_buf), {}, {});

    ScopedLock images_lock(m_images_mutex);
    const auto next_index = m_images.size();
    auto sampled_image = image.full_view().sampled(to_sampler(mag_filter, min_filter, wrap_u, wrap_v));
    m_images.push(vull::move(image));
    vk::DescriptorBuilder descriptor_builder(m_set_layout, m_descriptor_buffer);
    descriptor_builder.set(0, next_index, sampled_image);
    return next_index;
}

uint32_t TextureStreamer::load_texture(const String &name, uint32_t fallback_index) {
    auto stream = vpak::open(name);
    if (!stream) {
        vull::error("[graphics] Failed to find texture {}", name);
        return fallback_index;
    }

    if (auto index = load_texture(*stream).to_optional()) {
        return *index;
    }
    vull::error("[graphics] Failed to load texture {}", name);
    return fallback_index;
}

uint32_t TextureStreamer::ensure_texture(const String &name, TextureKind kind) {
    // First check if the texture is already loaded.
    if (auto index = m_loaded_indices.get(name)) {
        return *index;
    }

    // Otherwise check if there is a pending future.
    const uint32_t fallback_index = kind == TextureKind::Normal ? 1 : 0;
    if (auto future = m_futures.get(name)) {
        if (!future->is_complete()) {
            return fallback_index;
        }
        uint32_t index = future->await();
        m_loaded_indices.set(name, index);
        m_futures.remove(name);
        return index;
    }

    // There is no pending future so we need to schedule the load.
    auto future = tasklet::schedule([this, name, fallback_index] mutable {
        return load_texture(name, fallback_index);
    });
    m_futures.set(name, vull::move(future));
    return fallback_index;
}

} // namespace vull
