#include <vull/core/Scene.hh>

#include <vull/core/BoundingBox.hh>
#include <vull/core/Log.hh>
#include <vull/core/Transform.hh>
#include <vull/ecs/EntityId.hh>
#include <vull/ecs/World.hh>
#include <vull/graphics/Material.hh>
#include <vull/graphics/Mesh.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Mat.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/HashMap.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Result.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/vpak/PackFile.hh>
#include <vull/vpak/Reader.hh>
#include <vull/vulkan/Buffer.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Image.hh>
#include <vull/vulkan/MemoryUsage.hh>
#include <vull/vulkan/Queue.hh>
#include <vull/vulkan/Vulkan.hh>

// IWYU pragma: no_forward_declare vull::vkb::Image_T

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
    default:
        return {vkb::Format::Undefined, 0u, false};
    }
}

} // namespace

Scene::~Scene() {
    m_context.vkDestroySampler(m_nearest_sampler);
    m_context.vkDestroySampler(m_linear_sampler);
}

Mat4f Scene::get_transform_matrix(EntityId entity) {
    const auto &transform = m_world.get_component<Transform>(entity);
    if (transform.parent() == ~vull::EntityId(0)) {
        // Root node.
        return transform.matrix();
    }
    const auto parent_matrix = get_transform_matrix(transform.parent());
    return parent_matrix * transform.matrix();
}

vk::Image Scene::load_image(vpak::ReadStream &stream) {
    const auto [format, unit_size, block_compressed] = parse_format(VULL_EXPECT(stream.read_byte()));
    const auto sampler_kind = static_cast<vpak::SamplerKind>(VULL_EXPECT(stream.read_byte()));
    const auto width = VULL_EXPECT(stream.read_varint<uint32_t>());
    const auto height = VULL_EXPECT(stream.read_varint<uint32_t>());
    const auto mip_count = VULL_EXPECT(stream.read_varint<uint32_t>());

    // TODO: What's the best thing to do if this happens?
    uint32_t expected_mip_count = 32u - vull::clz(vull::max(width, height));
    if (mip_count != expected_mip_count) {
        vull::warn("[scene] Expected {} mips, but got {}", expected_mip_count, mip_count);
    }

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

    switch (sampler_kind) {
    case vpak::SamplerKind::LinearRepeat:
        m_texture_samplers.push(m_linear_sampler);
        break;
    default:
        vull::warn("[scene] Invalid sampler kind");
        [[fallthrough]];
    case vpak::SamplerKind::NearestRepeat:
        m_texture_samplers.push(m_nearest_sampler);
        break;
    }

    // Transition the whole image (all mip levels) to TransferDstOptimal.
    m_context.graphics_queue().immediate_submit([&image, mip_count](const vk::CommandBuffer &cmd_buf) {
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
        VULL_EXPECT(stream.read({staging_buffer.mapped_raw(), mip_size}));

        // Perform CPU -> GPU copy.
        m_context.graphics_queue().immediate_submit([&](const vk::CommandBuffer &cmd_buf) {
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

    // Transition the whole image to ShaderReadOnlyOptimal.
    m_context.graphics_queue().immediate_submit([&image, mip_count](const vk::CommandBuffer &cmd_buf) {
        vkb::ImageMemoryBarrier2 image_read_barrier{
            .sType = vkb::StructureType::ImageMemoryBarrier2,
            .srcStageMask = vkb::PipelineStage2::Copy,
            .srcAccessMask = vkb::Access2::TransferWrite,
            .dstStageMask = vkb::PipelineStage2::AllCommands,
            .dstAccessMask = vkb::Access2::ShaderRead,
            .oldLayout = vkb::ImageLayout::TransferDstOptimal,
            .newLayout = vkb::ImageLayout::ShaderReadOnlyOptimal,
            .image = *image,
            .subresourceRange{
                .aspectMask = vkb::ImageAspect::Color,
                .levelCount = mip_count,
                .layerCount = 1,
            },
        };
        cmd_buf.image_barrier(image_read_barrier);
    });
    return image;
}

void Scene::load(vpak::Reader &pack_reader, StringView scene_name) {
    vkb::SamplerCreateInfo linear_sampler_ci{
        .sType = vkb::StructureType::SamplerCreateInfo,
        .magFilter = vkb::Filter::Linear,
        .minFilter = vkb::Filter::Linear,
        .mipmapMode = vkb::SamplerMipmapMode::Linear,
        .addressModeU = vkb::SamplerAddressMode::Repeat,
        .addressModeV = vkb::SamplerAddressMode::Repeat,
        .addressModeW = vkb::SamplerAddressMode::Repeat,
        .anisotropyEnable = true,
        .maxAnisotropy = 16.0f,
        .maxLod = vkb::k_lod_clamp_none,
        .borderColor = vkb::BorderColor::FloatTransparentBlack,
    };
    VULL_ENSURE(m_context.vkCreateSampler(&linear_sampler_ci, &m_linear_sampler) == vkb::Result::Success);

    vkb::SamplerCreateInfo nearest_sampler_ci{
        .sType = vkb::StructureType::SamplerCreateInfo,
        .magFilter = vkb::Filter::Nearest,
        .minFilter = vkb::Filter::Nearest,
        .mipmapMode = vkb::SamplerMipmapMode::Linear,
        .addressModeU = vkb::SamplerAddressMode::Repeat,
        .addressModeV = vkb::SamplerAddressMode::Repeat,
        .addressModeW = vkb::SamplerAddressMode::Repeat,
        .anisotropyEnable = true,
        .maxAnisotropy = 16.0f,
        .maxLod = vkb::k_lod_clamp_none,
        .borderColor = vkb::BorderColor::FloatTransparentBlack,
    };
    VULL_ENSURE(m_context.vkCreateSampler(&nearest_sampler_ci, &m_nearest_sampler) == vkb::Result::Success);

    // Register default components. Note that the order currently matters.
    m_world.register_component<Transform>();
    m_world.register_component<Mesh>();
    m_world.register_component<Material>();
    m_world.register_component<BoundingBox>();

    // Load world.
    VULL_EXPECT(m_world.deserialise(pack_reader, scene_name));

    // Load textures.
    for (const auto &entry : pack_reader.entries()) {
        switch (entry.type) {
        case vpak::EntryType::Image:
            auto stream = pack_reader.open(entry.name);
            m_texture_indices.set(entry.name, m_texture_images.size());
            m_texture_images.push(load_image(*stream));
            break;
        }
    }
}

} // namespace vull
