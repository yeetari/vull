#include <vull/ui/font_atlas.hh>

#include <vull/container/vector.hh>
#include <vull/maths/common.hh>
#include <vull/maths/relational.hh>
#include <vull/maths/vec.hh>
#include <vull/support/assert.hh>
#include <vull/support/optional.hh>
#include <vull/support/span.hh>
#include <vull/support/tuple.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/future.hh>
#include <vull/ui/font.hh>
#include <vull/vulkan/buffer.hh>
#include <vull/vulkan/command_buffer.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/image.hh>
#include <vull/vulkan/memory.hh>
#include <vull/vulkan/queue.hh>
#include <vull/vulkan/sampler.hh>
#include <vull/vulkan/vulkan.hh>

#include <stdint.h>

namespace vull::ui {

FontAtlas::FontAtlas(vk::Context &context, Vec2u extent) : m_context(context), m_extent(extent) {
    vkb::ImageCreateInfo image_ci{
        .sType = vkb::StructureType::ImageCreateInfo,
        .imageType = vkb::ImageType::_2D,
        .format = vkb::Format::R8Unorm,
        .extent = {extent.x(), extent.y(), 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vkb::SampleCount::_1,
        .tiling = vkb::ImageTiling::Optimal,
        .usage = vkb::ImageUsage::TransferDst | vkb::ImageUsage::Sampled,
        .sharingMode = vkb::SharingMode::Exclusive,
        .initialLayout = vkb::ImageLayout::Undefined,
    };
    m_image = context.create_image(image_ci, vk::DeviceMemoryFlag::HighPriority);

    auto &queue = context.get_queue(vk::QueueKind::Graphics);
    auto cmd_buf = queue.request_cmd_buf();
    cmd_buf->image_barrier({
        .sType = vkb::StructureType::ImageMemoryBarrier2,
        .dstStageMask = vkb::PipelineStage2::Clear,
        .dstAccessMask = vkb::Access2::TransferWrite,
        .oldLayout = vkb::ImageLayout::Undefined,
        .newLayout = vkb::ImageLayout::TransferDstOptimal,
        .image = *m_image,
        .subresourceRange = m_image.full_view().range(),
    });

    vkb::ClearColorValue colour{
        .float32{},
    };
    const auto range = m_image.full_view().range();
    m_context.vkCmdClearColorImage(**cmd_buf, *m_image, vkb::ImageLayout::TransferDstOptimal, &colour, 1, &range);

    cmd_buf->image_barrier({
        .sType = vkb::StructureType::ImageMemoryBarrier2,
        .srcStageMask = vkb::PipelineStage2::Clear,
        .srcAccessMask = vkb::Access2::TransferWrite,
        .dstStageMask = vkb::PipelineStage2::AllGraphics,
        .dstAccessMask = vkb::Access2::ShaderSampledRead,
        .oldLayout = vkb::ImageLayout::TransferDstOptimal,
        .newLayout = vkb::ImageLayout::ReadOnlyOptimal,
        .image = *m_image,
        .subresourceRange = m_image.full_view().range(),
    });
    queue.submit(vull::move(cmd_buf), {}, {}).await();

    m_skyline = new Node{
        .width = extent.x(),
    };
}

FontAtlas::~FontAtlas() {
    for (auto *node = m_skyline; node != nullptr;) {
        delete vull::exchange(node, node->next);
    }
}

Optional<uint32_t> FontAtlas::pack_rect(Node *node, Vec2u extent) {
    if (node->offset.x() + extent.x() > m_extent.x()) {
        return {};
    }

    uint32_t max_x = node->offset.x() + extent.x();
    uint32_t min_y = 0;
    while (node->offset.x() < max_x) {
        min_y = vull::max(min_y, node->offset.y());
        node = node->next;
        if (node == nullptr) {
            break;
        }
    }
    return min_y;
}

Optional<Tuple<FontAtlas::Node **, Vec2u>> FontAtlas::find_rect(Vec2u extent) {
    Node **prev_link = &m_skyline;
    Node **best_link = nullptr;
    uint32_t best_y = UINT32_MAX;
    for (auto *node = m_skyline; node != nullptr; node = node->next) {
        if (node->offset.x() + extent.x() > m_extent.x()) {
            break;
        }

        auto min_y = pack_rect(node, extent);
        if (!min_y) {
            VULL_ASSERT(node->offset.x() + extent.x() > m_extent.x());
            break;
        }

        if (*min_y < best_y) {
            best_y = *min_y;
            best_link = prev_link;
        }
        prev_link = &node->next;
    }

    if (best_link == nullptr) {
        return {};
    }

    Vec2u offset((*best_link)->offset.x(), best_y);
    return vull::make_tuple(best_link, offset);
}

Optional<Vec2u> FontAtlas::allocate_rect(Vec2u extent) {
    const auto find_result = find_rect(extent);
    if (!find_result) {
        // TODO: LRU cache.
        return {};
    }
    auto [best_link, offset] = *find_result;

    auto *new_node = new Node{
        .offset = offset + Vec2u(0, extent.y()),
        .width = extent.x(),
    };

    Node *current = *best_link;
    if (current->offset.x() < offset.x()) {
        current = vull::exchange(current->next, new_node);
    } else {
        *best_link = new_node;
    }

    while (current->next != nullptr && current->next->offset.x() <= offset.x() + extent.x()) {
        delete vull::exchange(current, current->next);
    }

    new_node->next = current;
    current->offset.set_x(vull::max(current->offset.x(), offset.x() + extent.x()));
    return offset;
}

CachedGlyph FontAtlas::ensure_glyph(Font &font, uint32_t glyph_index) {
    // Ignore any bad indices.
    if (glyph_index >= font.glyph_count()) {
        return {};
    }

    // Try to find the glyph.
    // TODO: Replace linear search - maybe a binary tree that can also be used for rect packing?
    for (auto &glyph : m_cache) {
        if (glyph.font == &font && glyph.index == glyph_index) {
            if (!glyph.future.is_valid()) {
                // Future has already completed, we can return the index.
                return glyph;
            }
            if (glyph.future.is_complete()) {
                // Future has completed since our last check, clear it.
                glyph.future.await();
                glyph.future = {};
                return glyph;
            }
            // Otherwise the future is still pending.
            return {};
        }
    }

    // Otherwise the glyph has not been loaded.
    const auto glyph_info = font.ensure_glyph(glyph_index);
    if (vull::any(vull::equal(glyph_info.bitmap_extent, Vec2u(0)))) {
        return {};
    }

    const auto offset = allocate_rect(glyph_info.bitmap_extent);
    if (!offset) {
        return {};
    }

    const auto glyph_size = glyph_info.bitmap_extent.x() * glyph_info.bitmap_extent.y();
    auto staging_buffer = m_context.create_buffer(
        glyph_size, vkb::BufferUsage::TransferSrc,
        vk::DeviceMemoryFlags(vk::DeviceMemoryFlag::HostSequentialWrite, vk::DeviceMemoryFlag::Staging));
    font.rasterise(glyph_index, {staging_buffer.mapped<uint8_t>(), glyph_size});

    auto &queue = m_context.get_queue(vk::QueueKind::Graphics);
    auto cmd_buf = queue.request_cmd_buf();
    vkb::ImageMemoryBarrier2 transfer_write_barrier{
        .sType = vkb::StructureType::ImageMemoryBarrier2,
        .srcStageMask = vkb::PipelineStage2::AllCommands,
        .srcAccessMask = vkb::Access2::ShaderSampledRead,
        .dstStageMask = vkb::PipelineStage2::Copy,
        .dstAccessMask = vkb::Access2::TransferWrite,
        .oldLayout = vkb::ImageLayout::ReadOnlyOptimal,
        .newLayout = vkb::ImageLayout::TransferDstOptimal,
        .image = *m_image,
        .subresourceRange = m_image.full_view().range(),
    };
    cmd_buf->image_barrier(transfer_write_barrier);

    vkb::BufferImageCopy copy_region{
        .imageSubresource{
            .aspectMask = vkb::ImageAspect::Color,
            .layerCount = 1,
        },
        .imageOffset{
            .x = static_cast<int32_t>(offset->x()),
            .y = static_cast<int32_t>(offset->y()),
        },
        .imageExtent{
            .width = glyph_info.bitmap_extent.x(),
            .height = glyph_info.bitmap_extent.y(),
            .depth = 1,
        },
    };
    cmd_buf->copy_buffer_to_image(staging_buffer, m_image, vkb::ImageLayout::TransferDstOptimal, copy_region);

    vkb::ImageMemoryBarrier2 sampled_read_barrier{
        .sType = vkb::StructureType::ImageMemoryBarrier2,
        .srcStageMask = vkb::PipelineStage2::Copy,
        .srcAccessMask = vkb::Access2::TransferWrite,
        .dstStageMask = vkb::PipelineStage2::AllCommands,
        .dstAccessMask = vkb::Access2::ShaderSampledRead,
        .oldLayout = vkb::ImageLayout::TransferDstOptimal,
        .newLayout = vkb::ImageLayout::ReadOnlyOptimal,
        .image = *m_image,
        .subresourceRange = m_image.full_view().range(),
    };
    cmd_buf->image_barrier(sampled_read_barrier);

    cmd_buf->bind_associated_buffer(vull::move(staging_buffer));
    auto future = queue.submit(vull::move(cmd_buf), {}, {});
    m_cache.push({
        .font = &font,
        .index = glyph_index,
        .atlas_offset = *offset,
        .size = glyph_info.bitmap_extent,
        .bitmap_offset = glyph_info.bitmap_offset,
        .future = vull::move(future),
    });
    return {};
}

vk::SampledImage FontAtlas::sampled_image() const {
    return m_image
        .swizzle_view({
            .r = vkb::ComponentSwizzle::One,
            .g = vkb::ComponentSwizzle::One,
            .b = vkb::ComponentSwizzle::One,
            .a = vkb::ComponentSwizzle::R,
        })
        .sampled(vk::Sampler::Nearest);
}

} // namespace vull::ui
