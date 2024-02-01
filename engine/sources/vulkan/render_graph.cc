#include <vull/vulkan/render_graph.hh>

#include <vull/container/hash_map.hh>
#include <vull/container/vector.hh>
#include <vull/maths/common.hh>
#include <vull/support/assert.hh>
#include <vull/support/function.hh>
#include <vull/support/optional.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/tuple.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/vulkan/buffer.hh>
#include <vull/vulkan/command_buffer.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/image.hh>
#include <vull/vulkan/memory_usage.hh>
#include <vull/vulkan/query_pool.hh>
#include <vull/vulkan/render_graph_defs.hh>
#include <vull/vulkan/vulkan.hh>

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

Pass &Pass::read(ResourceId &id, ReadFlags flags) {
    m_reads.push(vull::make_tuple(id, flags));
    if ((flags & ReadFlags::Present) != ReadFlags::None) {
        // Create a new handle so that a present pass can be the target pass for compilation.
        id = m_graph.clone_resource(id, *this);
    }
    return *this;
}

Pass &Pass::write(ResourceId &id, WriteFlags flags) {
    if ((flags & WriteFlags::Additive) != WriteFlags::None) {
        // This pass doesn't fully overwrite the resource.
        m_reads.push(vull::make_tuple(id, ReadFlags::Additive));
    }
    id = m_graph.clone_resource(id, *this);
    m_writes.push(vull::make_tuple(id, flags));
    return *this;
}

Pass &RenderGraph::add_pass(String name, PassFlags flags) {
    return *m_passes.emplace(new Pass(*this, vull::move(name), flags));
}

ResourceId RenderGraph::import(String name, const Buffer &buffer) {
    return create_resource(vull::move(name), ResourceFlags::Buffer | ResourceFlags::Imported, [&buffer] {
        return &buffer;
    });
}

ResourceId RenderGraph::import(String name, const Image &image) {
    return create_resource(vull::move(name), ResourceFlags::Image | ResourceFlags::Imported, [&image] {
        return &image;
    });
}

ResourceId RenderGraph::new_attachment(String name, const AttachmentDescription &description) {
    return create_resource(vull::move(name), ResourceFlags::Image | ResourceFlags::Uninitialised,
                           [description, &context = m_context, image = vk::Image()]() mutable {
                               vkb::ImageCreateInfo image_ci{
                                   .sType = vkb::StructureType::ImageCreateInfo,
                                   .imageType = vkb::ImageType::_2D,
                                   .format = description.format,
                                   .extent = {description.extent.width, description.extent.height, 1},
                                   .mipLevels = description.mip_levels,
                                   .arrayLayers = description.array_layers,
                                   .samples = vkb::SampleCount::_1,
                                   .tiling = vkb::ImageTiling::Optimal,
                                   .usage = description.usage,
                                   .sharingMode = vkb::SharingMode::Exclusive,
                                   .initialLayout = vkb::ImageLayout::Undefined,
                               };
                               image = context.create_image(image_ci, vk::MemoryUsage::DeviceOnly);
                               return &image;
                           });
}

ResourceId RenderGraph::new_buffer(String name, const BufferDescription &description) {
    return create_resource(vull::move(name), ResourceFlags::Buffer | ResourceFlags::Uninitialised,
                           [description, &context = m_context, buffer = vk::Buffer()]() mutable {
                               const auto memory_usage = description.host_accessible ? vk::MemoryUsage::HostToDevice
                                                                                     : vk::MemoryUsage::DeviceOnly;
                               buffer = context.create_buffer(description.size, description.usage, memory_usage);
                               return &buffer;
                           });
}

const Buffer &RenderGraph::get_buffer(ResourceId id) {
    VULL_ASSERT((virtual_resource(id).flags() & ResourceFlags::Kind) == ResourceFlags::Buffer);
    return *static_cast<const Buffer *>(physical_resource(id).materialised());
}

const Image &RenderGraph::get_image(ResourceId id) {
    VULL_ASSERT((virtual_resource(id).flags() & ResourceFlags::Kind) == ResourceFlags::Image);
    return *static_cast<const Image *>(physical_resource(id).materialised());
}

RenderGraph::RenderGraph(Context &context) : m_context(context), m_timestamp_pool(context) {}

RenderGraph::~RenderGraph() {
    for (vkb::Event event : m_events) {
        m_context.vkDestroyEvent(event);
    }
}

ResourceId RenderGraph::create_resource(String &&name, ResourceFlags flags, Function<const void *()> &&materialise) {
    m_physical_resources.emplace(vull::move(name), vull::move(materialise));
    m_resources.emplace(nullptr, flags);
    return ResourceId(m_physical_resources.size() - 1, m_resources.size() - 1); // NOLINT
}

ResourceId RenderGraph::clone_resource(ResourceId id, Pass &producer) {
    const auto new_flags = virtual_resource(id).flags() & ~(ResourceFlags::Imported | ResourceFlags::Uninitialised);
    m_resources.emplace(&producer, new_flags);
    return ResourceId(id.physical_index(), m_resources.size() - 1); // NOLINT
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
            const auto &resource = virtual_resource(id);
            VULL_ASSERT((resource.flags() & ResourceFlags::Uninitialised) == ResourceFlags::None);
            if ((resource.flags() & ResourceFlags::Imported) != ResourceFlags::None) {
                // Imported resources have no producer.
                continue;
            }
            traverse(resource.producer());
        }
        m_pass_order.push(pass);
    };
    traverse(virtual_resource(target).producer());
}

void RenderGraph::build_sync() {
    // Build resource write info.
    for (auto &resource : m_resources) {
        if ((resource.flags() & (ResourceFlags::Imported | ResourceFlags::Uninitialised)) != ResourceFlags::None) {
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

        if (kind == PassFlags::Compute) {
            resource.set_write_stage(vkb::PipelineStage2::ComputeShader);
            resource.set_write_access(vkb::Access2::ShaderStorageWrite);

            // Writes to images in a compute shader are via storage images, which must be in the General layout.
            resource.set_write_layout(vkb::ImageLayout::General);
            continue;
        }

        // Otherwise the write is the attachment output of a fragment shader.
        resource.set_write_layout(vkb::ImageLayout::AttachmentOptimal);

        // TODO: Gather more information to be more granular here.
        resource.set_write_stage(vkb::PipelineStage2::AllGraphics);
        resource.set_write_access(vkb::Access2::MemoryWrite);
    }

    HashMap<uint16_t, vkb::ImageLayout> layout_map;
    for (Pass &pass : m_pass_order) {
        if ((pass.flags() & PassFlags::Kind) == PassFlags::Transfer) {
            pass.m_dst_stage |= vkb::PipelineStage2::AllTransfer;
            pass.m_dst_access |= vkb::Access2::TransferRead;
        }

        for (auto [id, flags] : pass.reads()) {
            if ((flags & ReadFlags::Additive) != ReadFlags::None) {
                continue;
            }
            const auto &resource = virtual_resource(id);
            if ((flags & ReadFlags::Indirect) != ReadFlags::None) {
                pass.m_dst_stage |= vkb::PipelineStage2::DrawIndirect;
                pass.m_dst_access |= vkb::Access2::IndirectCommandRead;
            }
            if ((resource.flags() & ResourceFlags::Kind) == ResourceFlags::Image) {
                const auto current_layout = *layout_map.get(id.physical_index());
                auto read_layout = vkb::ImageLayout::ReadOnlyOptimal;
                if ((flags & ReadFlags::Present) != ReadFlags::None) {
                    read_layout = vkb::ImageLayout::PresentSrcKHR;
                } else if ((pass.flags() & PassFlags::Kind) == PassFlags::Transfer) {
                    read_layout = vkb::ImageLayout::TransferSrcOptimal;
                }
                if (current_layout != read_layout) {
                    pass.add_transition(id, current_layout, read_layout);
                    layout_map.set(id.physical_index(), read_layout);
#ifdef RG_DEBUG
                    vull::debug("'{}' read '{}': {} -> {}", pass.name(), physical_resource(id).name(),
                                vull::to_underlying(current_layout), vull::to_underlying(read_layout));
#endif
                }
            }
        }
        for (auto [id, flags] : pass.writes()) {
            const auto &resource = virtual_resource(id);
            VULL_ASSERT(&resource.producer() == &pass);
            if ((resource.flags() & ResourceFlags::Kind) == ResourceFlags::Image) {
                const auto current_layout = layout_map.get(id.physical_index()).value_or(vkb::ImageLayout::Undefined);
                if (current_layout == resource.write_layout()) {
                    continue;
                }
                pass.add_transition(id, current_layout, resource.write_layout());
                layout_map.set(id.physical_index(), resource.write_layout());
#ifdef RG_DEBUG
                vull::debug("'{}' write '{}': {} -> {}", pass.name(), physical_resource(id).name(),
                            vull::to_underlying(current_layout), vull::to_underlying(resource.write_layout()));
#endif
            }
        }
    }
}

void RenderGraph::compile(ResourceId target) {
#ifdef RG_DEBUG
    vull::debug("RenderGraph::compile({})", physical_resource(target).name());
#endif
    // TODO: Graph validation.
    build_order(target);
    build_sync();

    // TODO: Recycle events.
    m_events.ensure_size(m_resources.size());
    for (uint16_t i = 0; i < m_resources.size(); i++) {
        vkb::EventCreateInfo event_ci{
            .sType = vkb::StructureType::EventCreateInfo,
            .flags = vkb::EventCreateFlags::DeviceOnly,
        };
        m_context.vkCreateEvent(&event_ci, &m_events[i]);
    }
}

void RenderGraph::record_pass(CommandBuffer &cmd_buf, Pass &pass) {
#ifdef RG_DEBUG
    vull::debug("RenderGraph::record_pass({})", pass.name());
#endif
    cmd_buf.begin_label(vull::format("Pass {}", pass.name()));

    // TODO(small-vector)
    Vector<vkb::Event> wait_events;
    for (auto [id, flags] : pass.reads()) {
        wait_events.push(m_events[id.virtual_index()]);
    }

    if (!wait_events.empty()) {
        Vector<vkb::MemoryBarrier2> wait_barriers;
        for (auto [id, flags] : pass.reads()) {
            wait_barriers.push({
                .sType = vkb::StructureType::MemoryBarrier2,
                .srcStageMask = virtual_resource(id).write_stage(),
                .srcAccessMask = virtual_resource(id).write_access(),
                .dstStageMask = pass.m_dst_stage,
                .dstAccessMask = pass.m_dst_access,
            });
        }
        Vector<vkb::DependencyInfo> wait_dependency_infos;
        for (uint32_t i = 0; i < pass.reads().size(); i++) {
            wait_dependency_infos.push({
                .sType = vkb::StructureType::DependencyInfo,
                .memoryBarrierCount = 1,
                .pMemoryBarriers = &wait_barriers[i],
            });
        }
        m_context.vkCmdWaitEvents2(*cmd_buf, wait_events.size(), wait_events.data(), wait_dependency_infos.data());
    }

    if (!pass.m_transitions.empty()) {
        Vector<vkb::ImageMemoryBarrier2> image_barriers;
        for (const auto &transition : pass.m_transitions) {
            const auto &image = get_image(transition.id);
            image_barriers.push({
                .sType = vkb::StructureType::ImageMemoryBarrier2,
                .dstStageMask = vkb::PipelineStage2::AllCommands,
                .dstAccessMask = vkb::Access2::MemoryRead,
                .oldLayout = transition.old_layout,
                .newLayout = transition.new_layout,
                .image = *image,
                .subresourceRange = image.full_view().range(),
            });
        }

        cmd_buf.pipeline_barrier({
            .sType = vkb::StructureType::DependencyInfo,
            .imageMemoryBarrierCount = image_barriers.size(),
            .pImageMemoryBarriers = image_barriers.data(),
        });
    }

    // Begin dynamic rendering state.
    if ((pass.flags() & PassFlags::Kind) == PassFlags::Graphics) {
        Vector<vkb::RenderingAttachmentInfo> colour_attachments;
        Optional<vkb::RenderingAttachmentInfo> depth_attachment;
        vkb::Extent2D extent{};

        auto consider_resource = [&](ResourceId id, vkb::AttachmentLoadOp load_op, vkb::AttachmentStoreOp store_op) {
            if ((virtual_resource(id).flags() & ResourceFlags::Kind) != ResourceFlags::Image) {
                return;
            }

            const auto &image = get_image(id);
            extent.width = vull::max(extent.width, image.extent().width);
            extent.height = vull::max(extent.height, image.extent().height);

            const bool is_colour = image.full_view().range().aspectMask == vkb::ImageAspect::Color;
#ifdef RG_DEBUG
            vull::debug("{} attachment '{}' (load_op: {}, store_op: {})", is_colour ? "colour" : "depth",
                        physical_resource(id).name(), vull::to_underlying(load_op), vull::to_underlying(store_op));
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

        for (auto [id, flags] : pass.reads()) {
            if ((flags & (ReadFlags::Additive | ReadFlags::Sampled)) != ReadFlags::None) {
                // Ignore Additive reads as they are handled by vkb::AttachmentLoadOp::Load on the write side. Ignore
                // non-attachment Sampled reads.
                continue;
            }
            consider_resource(id, vkb::AttachmentLoadOp::Load, vkb::AttachmentStoreOp::None);
        }
        for (auto [id, flags] : pass.writes()) {
            // TODO: How to choose Clear vs DontCare for load op?
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

    // Execute user-provided record function.
    if (pass.m_on_execute) {
        pass.m_on_execute(cmd_buf);
    }

    if ((pass.flags() & PassFlags::Kind) == PassFlags::Graphics) {
        cmd_buf.end_rendering();
    }

    for (auto [id, flags] : pass.writes()) {
        vkb::MemoryBarrier2 memory_barrier{
            .sType = vkb::StructureType::MemoryBarrier2,
            .srcStageMask = virtual_resource(id).write_stage(),
        };
        vkb::DependencyInfo dependency_info{
            .sType = vkb::StructureType::DependencyInfo,
            .memoryBarrierCount = 1,
            .pMemoryBarriers = &memory_barrier,
        };
        m_context.vkCmdSetEvent2(*cmd_buf, m_events[id.virtual_index()], &dependency_info);
#ifdef RG_DEBUG
        vull::debug("'{}' signal for '{}'", pass.name(), physical_resource(id).name());
#endif
    }

    cmd_buf.end_label();
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
