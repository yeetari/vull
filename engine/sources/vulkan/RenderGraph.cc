#include <vull/vulkan/RenderGraph.hh>

#include <vull/maths/Common.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Function.hh>
#include <vull/support/HashMap.hh>
#include <vull/support/Optional.hh>
#include <vull/support/String.hh>
#include <vull/support/Tuple.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/Buffer.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Image.hh>
#include <vull/vulkan/MemoryUsage.hh>
#include <vull/vulkan/QueryPool.hh>
#include <vull/vulkan/RenderGraphDefs.hh>
#include <vull/vulkan/Vulkan.hh>

// TODO: Need a render graph cache to have this on by default, otherwise stuff gets logged every frame.
// #define RG_DEBUG
#ifdef RG_DEBUG
#include <vull/core/Log.hh>
#endif

namespace vull::vk {

const void *PhysicalResource::materialised() {
    if (m_materialised == nullptr) {
        m_materialised = m_materialise();
    }
    return m_materialised;
}

void Pass::add_transition(ResourceId id, vkb::ImageLayout old_layout, vkb::ImageLayout new_layout) {
    m_transitions.push({id, old_layout, new_layout});
}

vkb::DependencyInfo Pass::dependency_info(RenderGraph &graph, Vector<vkb::ImageMemoryBarrier2> &image_barriers) const {
    for (const auto &transition : m_transitions) {
        const auto &image = graph.get_image(transition.id);
        image_barriers.push({
            .sType = vkb::StructureType::ImageMemoryBarrier2,
            .srcStageMask = m_memory_barrier.srcStageMask,
            .srcAccessMask = m_memory_barrier.srcAccessMask,
            .dstStageMask = m_memory_barrier.dstStageMask,
            .dstAccessMask = m_memory_barrier.dstAccessMask,
            .oldLayout = transition.old_layout,
            .newLayout = transition.new_layout,
            .image = *image,
            .subresourceRange = image.full_view().range(),
        });
    }

    const bool has_memory_barrier = m_memory_barrier.srcStageMask != vkb::PipelineStage2::None &&
                                    m_memory_barrier.dstStageMask != vkb::PipelineStage2::None;
    return {
        .sType = vkb::StructureType::DependencyInfo,
        .memoryBarrierCount = has_memory_barrier ? 1u : 0u,
        .pMemoryBarriers = &m_memory_barrier,
        .imageMemoryBarrierCount = image_barriers.size(),
        .pImageMemoryBarriers = image_barriers.data(),
    };
}

ResourceId PassBuilder::new_attachment(String name, const AttachmentDescription &description) {
    const auto id = m_graph.create_resource(vull::move(name), ResourceFlags::Image, &m_pass,
                                            [description, &context = m_graph.context(), image = vk::Image()]() mutable {
                                                vkb::ImageCreateInfo image_ci{
                                                    .sType = vkb::StructureType::ImageCreateInfo,
                                                    .imageType = vkb::ImageType::_2D,
                                                    .format = description.format,
                                                    .extent = {description.extent.width, description.extent.height, 1},
                                                    .mipLevels = description.mip_levels,
                                                    .arrayLayers = 1,
                                                    .samples = vkb::SampleCount::_1,
                                                    .tiling = vkb::ImageTiling::Optimal,
                                                    .usage = description.usage,
                                                    .sharingMode = vkb::SharingMode::Exclusive,
                                                    .initialLayout = vkb::ImageLayout::Undefined,
                                                };
                                                image = context.create_image(image_ci, vk::MemoryUsage::DeviceOnly);
                                                return &image;
                                            });
    return m_pass.m_creates.emplace(id);
}

ResourceId PassBuilder::new_buffer(String name, const BufferDescription &description) {
    const auto id =
        m_graph.create_resource(vull::move(name), ResourceFlags::Buffer, &m_pass,
                                [description, &context = m_graph.context(), buffer = vk::Buffer()]() mutable {
                                    const auto memory_usage = description.host_accessible
                                                                  ? vk::MemoryUsage::HostToDevice
                                                                  : vk::MemoryUsage::DeviceOnly;
                                    buffer = context.create_buffer(description.size, description.usage, memory_usage);
                                    return &buffer;
                                });
    return m_pass.m_creates.emplace(id);
}

ResourceId PassBuilder::read(ResourceId id, ReadFlags flags) {
    m_pass.m_reads.push(vull::make_tuple(id, flags));
    if ((flags & ReadFlags::Present) != ReadFlags::None) {
        // Create a new handle so that a present pass can be the target pass for compilation.
        id = m_graph.clone_resource(id, m_pass);
    }
    return id;
}

ResourceId PassBuilder::write(ResourceId id, WriteFlags flags) {
    if ((flags & WriteFlags::Additive) != WriteFlags::None) {
        // This pass doesn't fully overwrite the resource.
        m_pass.m_reads.push(vull::make_tuple(id, ReadFlags::Additive));
    }
    id = m_graph.clone_resource(id, m_pass);
    m_pass.m_writes.push(vull::make_tuple(id, flags));
    return id;
}

ResourceId RenderGraph::import(String name, const Buffer &buffer) {
    return create_resource(vull::move(name), ResourceFlags::Buffer | ResourceFlags::Imported, nullptr, [&buffer] {
        return &buffer;
    });
}

ResourceId RenderGraph::import(String name, const Image &image) {
    return create_resource(vull::move(name), ResourceFlags::Image | ResourceFlags::Imported, nullptr, [&image] {
        return &image;
    });
}

const Buffer &RenderGraph::get_buffer(ResourceId id) {
    auto &resource = m_resources[id];
    VULL_ASSERT((resource.flags() & ResourceFlags::Kind) == ResourceFlags::Buffer);
    return *static_cast<const Buffer *>(m_physical_resources[resource.physical_index()].materialised());
}

const Image &RenderGraph::get_image(ResourceId id) {
    auto &resource = m_resources[id];
    VULL_ASSERT((resource.flags() & ResourceFlags::Kind) == ResourceFlags::Image);
    return *static_cast<const Image *>(m_physical_resources[resource.physical_index()].materialised());
}

RenderGraph::RenderGraph(Context &context) : m_context(context), m_timestamp_pool(context) {}
RenderGraph::~RenderGraph() = default;

ResourceId RenderGraph::create_resource(String &&name, ResourceFlags flags, Pass *producer,
                                        Function<const void *()> &&materialise) {
    m_physical_resources.emplace(vull::move(name), vull::move(materialise));
    m_resources.emplace(producer, flags, m_physical_resources.size() - 1);
    return m_resources.size() - 1;
}

ResourceId RenderGraph::clone_resource(ResourceId id, Pass &producer) {
    const auto new_flags = m_resources[id].flags() & ~ResourceFlags::Imported;
    m_resources.emplace(&producer, new_flags, m_resources[id].physical_index());
    return m_resources.size() - 1;
}

void RenderGraph::build_order(ResourceId target) {
    // Post-order traversal to build a linear pass order, starting from the producer of the target resource.
    // TODO: Passes with no side effects other than writing to imported resources will be culled.
    Function<void(Pass &)> traverse = [&](Pass &pass) {
        if (vull::exchange(pass.m_visited, true)) {
            return;
        }
        // Traverse dependant passes (those that produce a resource we read from).
        for (auto [id, flags] : pass.reads()) {
            const auto &resource = m_resources[id];
            if ((resource.flags() & ResourceFlags::Imported) != ResourceFlags::None) {
                // Imported resources have no producer.
                continue;
            }
            traverse(resource.producer());
        }
        m_pass_order.push(pass);
    };
    traverse(m_resources[target].producer());
}

void RenderGraph::build_sync() {
    // Build resource write info.
    for (auto &resource : m_resources) {
        if ((resource.flags() & ResourceFlags::Imported) != ResourceFlags::None) {
            continue;
        }

        const auto &producer = resource.producer();
        const auto kind = producer.flags() & PassFlags::Kind;
        if (kind == PassFlags::Transfer) {
            resource.set_write_stage(vkb::PipelineStage2::AllTransfer);
            resource.set_write_access(vkb::Access2::TransferWrite);
            resource.set_write_layout(vkb::ImageLayout::TransferDstOptimal);
            continue;
        }

        // TODO: Proper stage+access selection.
        resource.set_write_stage(vkb::PipelineStage2::AllCommands);
        resource.set_write_access(vkb::Access2::MemoryWrite);
        if (kind == PassFlags::Compute) {
            // Writes to images in a compute shader are via storage images, which must be in the General layout.
            resource.set_write_layout(vkb::ImageLayout::General);
            continue;
        }
        resource.set_write_layout(vkb::ImageLayout::AttachmentOptimal);
    }

    HashMap<vk::ResourceId, vkb::ImageLayout> layout_map;
    for (Pass &pass : m_pass_order) {
        for (auto id : pass.creates()) {
            const auto &resource = m_resources[id];
            if ((resource.flags() & ResourceFlags::Kind) == ResourceFlags::Image) {
                pass.add_transition(id, vkb::ImageLayout::Undefined, resource.write_layout());
                layout_map.set(resource.physical_index(), resource.write_layout());
#ifdef RG_DEBUG
                vull::debug("'{}' create '{}': Undefined -> {}", pass.name(),
                            m_physical_resources[resource.physical_index()].name(),
                            vull::to_underlying(resource.write_layout()));
#endif
            }
        }

        auto &barrier = pass.m_memory_barrier;
        if ((pass.flags() & PassFlags::Kind) == PassFlags::Transfer) {
            barrier.dstStageMask |= vkb::PipelineStage2::AllTransfer;
            barrier.dstAccessMask |= vkb::Access2::TransferRead;
        }

        for (auto [id, flags] : pass.reads()) {
            if ((flags & ReadFlags::Additive) != ReadFlags::None) {
                continue;
            }
            const auto &resource = m_resources[id];
            barrier.srcStageMask |= resource.write_stage();
            barrier.srcAccessMask |= resource.write_access();
            if ((flags & ReadFlags::Indirect) != ReadFlags::None) {
                barrier.dstStageMask |= vkb::PipelineStage2::DrawIndirect;
                barrier.dstAccessMask |= vkb::Access2::IndirectCommandRead;
            }
            if ((resource.flags() & ResourceFlags::Kind) == ResourceFlags::Image) {
                const auto current_layout = *layout_map.get(resource.physical_index());
                auto read_layout = vkb::ImageLayout::ReadOnlyOptimal;
                if ((flags & ReadFlags::Present) != ReadFlags::None) {
                    read_layout = vkb::ImageLayout::PresentSrcKHR;
                } else if ((pass.flags() & PassFlags::Kind) == PassFlags::Transfer) {
                    read_layout = vkb::ImageLayout::TransferSrcOptimal;
                }
                if (current_layout != read_layout) {
                    pass.add_transition(id, current_layout, read_layout);
                    layout_map.set(resource.physical_index(), read_layout);
#ifdef RG_DEBUG
                    vull::debug("'{}' read '{}': {} -> {}", pass.name(),
                                m_physical_resources[resource.physical_index()].name(),
                                vull::to_underlying(current_layout), vull::to_underlying(read_layout));
#endif
                }
            }
        }
        for (auto [id, flags] : pass.writes()) {
            const auto &resource = m_resources[id];
            VULL_ASSERT(&resource.producer() == &pass);
            barrier.srcStageMask |= resource.write_stage();
            barrier.srcAccessMask |= resource.write_access();
            barrier.dstStageMask |= resource.write_stage();
            barrier.dstAccessMask |= resource.write_access();
            if ((resource.flags() & ResourceFlags::Kind) == ResourceFlags::Image) {
                const auto current_layout =
                    layout_map.get(resource.physical_index()).value_or(vkb::ImageLayout::Undefined);
                if (current_layout == resource.write_layout()) {
                    continue;
                }
                pass.add_transition(id, current_layout, resource.write_layout());
                layout_map.set(resource.physical_index(), resource.write_layout());
#ifdef RG_DEBUG
                vull::debug("'{}' write '{}': {} -> {}", pass.name(),
                            m_physical_resources[resource.physical_index()].name(), vull::to_underlying(current_layout),
                            vull::to_underlying(resource.write_layout()));
#endif
            }
        }
    }
}

void RenderGraph::compile(ResourceId target) {
#ifdef RG_DEBUG
    vull::debug("RenderGraph::compile({})", m_physical_resources[m_resources[target].physical_index()].name());
#endif
    // TODO: Graph validation.
    build_order(target);
    build_sync();
}

void RenderGraph::record_pass(CommandBuffer &cmd_buf, Pass &pass) {
#ifdef RG_DEBUG
    vull::debug("RenderGraph::record_pass({})", pass.name());
#endif

    // Emit barrier.
    Vector<vkb::ImageMemoryBarrier2> image_barriers;
    const auto dependency_info = pass.dependency_info(*this, image_barriers);
    if (dependency_info.memoryBarrierCount + dependency_info.imageMemoryBarrierCount != 0) {
        cmd_buf.pipeline_barrier(dependency_info);
    }

    // Begin dynamic rendering state.
    if ((pass.flags() & PassFlags::Kind) == PassFlags::Graphics) {
        Vector<vkb::RenderingAttachmentInfo> colour_attachments;
        Optional<vkb::RenderingAttachmentInfo> depth_attachment;
        vkb::Extent2D extent{};

        auto consider_resource = [&](ResourceId id, vkb::AttachmentLoadOp load_op, vkb::AttachmentStoreOp store_op) {
            if ((m_resources[id].flags() & ResourceFlags::Kind) != ResourceFlags::Image) {
                return;
            }

            const auto &image = get_image(id);
            extent.width = vull::max(extent.width, image.extent().width);
            extent.height = vull::max(extent.height, image.extent().height);

            const bool is_colour = image.full_view().range().aspectMask == vkb::ImageAspect::Color;
#ifdef RG_DEBUG
            const auto &physical = m_physical_resources[m_resources[id].physical_index()];
            vull::debug("{} attachment '{}' (load_op: {}, store_op: {})", is_colour ? "colour" : "depth",
                        physical.name(), vull::to_underlying(load_op), vull::to_underlying(store_op));
#endif

            // TODO: Allow view other than full_view + custom clear value.
            vkb::RenderingAttachmentInfo attachment_info{
                .sType = vkb::StructureType::RenderingAttachmentInfo,
                .imageView = *image.full_view(),
                .imageLayout = vkb::ImageLayout::AttachmentOptimal,
                .loadOp = load_op,
                .storeOp = store_op,
            };
            if (is_colour) {
                colour_attachments.push(attachment_info);
            } else {
                VULL_ASSERT(!depth_attachment);
                depth_attachment.emplace(attachment_info);
            }
        };

        // TODO: How to choose Clear vs DontCare for load op?
        for (auto id : pass.creates()) {
            consider_resource(id, vkb::AttachmentLoadOp::Clear, vkb::AttachmentStoreOp::Store);
        }
        for (auto [id, flags] : pass.reads()) {
            if ((flags & (ReadFlags::Additive | ReadFlags::Sampled)) != ReadFlags::None) {
                // Ignore Additive reads as they are handled by vkb::AttachmentLoadOp::Load on the write side. Ignore
                // non-attachment Sampled reads.
                continue;
            }
            consider_resource(id, vkb::AttachmentLoadOp::Load, vkb::AttachmentStoreOp::None);
        }
        for (auto [id, flags] : pass.writes()) {
            const bool additive = (flags & WriteFlags::Additive) != WriteFlags::None;
            consider_resource(id, additive ? vkb::AttachmentLoadOp::Load : vkb::AttachmentLoadOp::Clear,
                              vkb::AttachmentStoreOp::Store);
        }

        vkb::RenderingInfo rendering_info{
            .sType = vkb::StructureType::RenderingInfo,
            .renderArea{
                .extent = extent,
            },
            .layerCount = 1,
            .colorAttachmentCount = colour_attachments.size(),
            .pColorAttachments = colour_attachments.data(),
            .pDepthAttachment = depth_attachment ? &*depth_attachment : nullptr,
        };
        cmd_buf.begin_rendering(rendering_info);

        vkb::Viewport viewport{
            .width = static_cast<float>(extent.width),
            .height = static_cast<float>(extent.height),
            .maxDepth = 1.0f,
        };
        vkb::Rect2D scissor{
            .extent = extent,
        };
        cmd_buf.set_viewport(viewport);
        cmd_buf.set_scissor(scissor);
    }

    pass.execute(*this, cmd_buf);

    if ((pass.flags() & PassFlags::Kind) == PassFlags::Graphics) {
        cmd_buf.end_rendering();
    }
}

void RenderGraph::execute(CommandBuffer &cmd_buf, bool record_timestamps) {
#ifdef RG_DEBUG
    vull::debug("RenderGraph::execute({})", record_timestamps);
#endif
    if (record_timestamps) {
        m_timestamp_pool.recreate(m_pass_order.size() + 1, vkb::QueryType::Timestamp);
        cmd_buf.reset_query_pool(m_timestamp_pool);
        cmd_buf.write_timestamp(vkb::PipelineStage2::None, m_timestamp_pool, 0);
    }
    for (uint32_t i = 0; i < m_pass_order.size(); i++) {
        Pass &pass = m_pass_order[i];
        record_pass(cmd_buf, pass);
        if (record_timestamps && (pass.flags() & PassFlags::Kind) != PassFlags::None) {
            // TODO(best-practices): Don't use AllCommands.
            cmd_buf.write_timestamp(vkb::PipelineStage2::AllCommands, m_timestamp_pool, i + 1);
        }
    }
}

// NOLINTNEXTLINE
String RenderGraph::to_json() const {
    VULL_ENSURE_NOT_REACHED("TODO");
}

} // namespace vull::vk
