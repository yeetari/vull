#include <vull/vulkan/RenderGraph.hh>

#include <vull/support/Algorithm.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Function.hh>
#include <vull/support/Optional.hh>
#include <vull/support/String.hh>
#include <vull/support/StringBuilder.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Tuple.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stddef.h>

namespace vull::vk {
namespace {

Tuple<vkb::PipelineStage2, vkb::Access2> read_stage(const Pass &pass, Resource &resource) {
    if (pass.kind() == PassKind::Compute) {
        // TODO: Finer grained access.
        return {vkb::PipelineStage2::ComputeShader, vkb::Access2::ShaderRead};
    }
    if (auto image = resource.as_image()) {
        if ((image->full_range().aspectMask & vkb::ImageAspect::Depth) != vkb::ImageAspect::None) {
            return {vkb::PipelineStage2::EarlyFragmentTests | vkb::PipelineStage2::LateFragmentTests,
                    vkb::Access2::DepthStencilAttachmentRead};
        }
    }
    VULL_ENSURE_NOT_REACHED("TODO");
}

vkb::ImageLayout write_layout(const Pass &writer) {
    // Writes to images in a compute shader are via storage images, and thus must be in the General layout.
    if (writer.kind() == PassKind::Compute) {
        return vkb::ImageLayout::General;
    }
    return vkb::ImageLayout::AttachmentOptimal;
}

Tuple<vkb::PipelineStage2, vkb::Access2> write_stage(const Pass &pass, Resource &resource) {
    if (pass.kind() == PassKind::Compute) {
        return {vkb::PipelineStage2::ComputeShader, vkb::Access2::ShaderWrite};
    }

    if (auto image = resource.as_image()) {
        const auto aspect = image->full_range().aspectMask;
        if ((aspect & vkb::ImageAspect::Color) != vkb::ImageAspect::None) {
            return {vkb::PipelineStage2::ColorAttachmentOutput, vkb::Access2::ColorAttachmentWrite};
        }
        if ((aspect & vkb::ImageAspect::Depth) != vkb::ImageAspect::None) {
            return {vkb::PipelineStage2::EarlyFragmentTests | vkb::PipelineStage2::LateFragmentTests,
                    vkb::Access2::DepthStencilAttachmentWrite};
        }
        VULL_ENSURE_NOT_REACHED();
    }
    return {vkb::PipelineStage2::AllGraphics, vkb::Access2::ShaderWrite};
}

StringView access_string(vkb::Access2 access) {
    switch (access) {
    case vkb::Access2::None:
        return "None";
    case vkb::Access2::ShaderRead:
        return "ShaderRead";
    case vkb::Access2::ShaderWrite:
        return "ShaderWrite";
    case vkb::Access2::ColorAttachmentWrite:
        return "ColorAttachmentWrite";
    case vkb::Access2::DepthStencilAttachmentWrite:
        return "DepthStencilAttachmentWrite";
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

StringView layout_string(vkb::ImageLayout layout) {
    switch (layout) {
    case vkb::ImageLayout::Undefined:
        return "Undefined";
    case vkb::ImageLayout::General:
        return "General";
    case vkb::ImageLayout::ReadOnlyOptimal:
        return "ReadOnlyOptimal";
    case vkb::ImageLayout::AttachmentOptimal:
        return "AttachmentOptimal";
    case vkb::ImageLayout::PresentSrcKHR:
        return "PresentSrc";
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

StringView stage_string(vkb::PipelineStage2 stage) {
    switch (stage) {
    case vkb::PipelineStage2::None:
        return "None";
    case vkb::PipelineStage2::VertexShader:
        return "VertexShader";
    case vkb::PipelineStage2::FragmentShader:
        return "FragmentShader";
    case vkb::PipelineStage2::EarlyFragmentTests | vkb::PipelineStage2::LateFragmentTests:
        return "EarlyFragmentTests | LateFragmentTests";
    case vkb::PipelineStage2::ColorAttachmentOutput:
        return "ColorAttachmentOutput";
    case vkb::PipelineStage2::ComputeShader:
        return "ComputeShader";
    case vkb::PipelineStage2::AllCommands:
        return "AllCommands";
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

} // namespace

vkb::BufferMemoryBarrier2 GenericBarrier::buffer_barrier(vkb::Buffer buffer) const {
    return {
        .sType = vkb::StructureType::BufferMemoryBarrier2,
        .srcStageMask = src_stage,
        .srcAccessMask = src_access,
        .dstStageMask = dst_stage,
        .dstAccessMask = dst_access,
        .buffer = buffer,
        .offset = buffer_offset,
        .size = vkb::k_whole_size,
    };
}

vkb::ImageMemoryBarrier2 GenericBarrier::image_barrier(vkb::Image image) const {
    return {
        .sType = vkb::StructureType::ImageMemoryBarrier2,
        .srcStageMask = src_stage,
        .srcAccessMask = src_access,
        .dstStageMask = dst_stage,
        .dstAccessMask = dst_access,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .image = image,
        .subresourceRange = subresource_range,
    };
}

void Pass::reads_from(Resource &resource) {
    m_reads.push({.resource = resource});
    resource.m_readers.push(*this);
}

void Pass::writes_to(Resource &resource) {
    m_writes.push({.resource = resource});
    resource.m_writers.push(*this);
}

bool Pass::does_write_to(Resource &resource) {
    for (auto &write : m_writes) {
        if (&write.resource == &resource) {
            return true;
        }
    }
    return false;
}

void Pass::record(const CommandBuffer &cmd_buf, Optional<const QueryPool &> timestamp_pool) {
    Vector<vkb::BufferMemoryBarrier2> buffer_barriers;
    Vector<vkb::ImageMemoryBarrier2> image_barriers;
    auto add_barriers = [&](const Vector<ResourceUse> &uses) {
        for (const auto &use : uses) {
            if (!use.barrier) {
                continue;
            }
            if (auto buffer = use.resource.as_buffer()) {
                buffer_barriers.push(use.barrier->buffer_barrier(*buffer));
            } else if (auto image = use.resource.as_image()) {
                image_barriers.push(use.barrier->image_barrier(*image));
            }
        }
    };
    add_barriers(m_reads);
    add_barriers(m_writes);

    vkb::DependencyInfo dependency_info{
        .sType = vkb::StructureType::DependencyInfo,
        .bufferMemoryBarrierCount = buffer_barriers.size(),
        .pBufferMemoryBarriers = buffer_barriers.data(),
        .imageMemoryBarrierCount = image_barriers.size(),
        .pImageMemoryBarriers = image_barriers.data(),
    };
    cmd_buf.pipeline_barrier(dependency_info);
    if (m_order_index != 0 && timestamp_pool) {
        cmd_buf.write_timestamp(vkb::PipelineStage2::AllCommands, *timestamp_pool, m_order_index);
    }
    if (m_on_record) {
        m_on_record(cmd_buf);
    }
}

Pass &RenderGraph::add_compute_pass(String name) {
    return *m_passes.emplace(new Pass(PassKind::Compute, vull::move(name)));
}

Pass &RenderGraph::add_graphics_pass(String name) {
    return *m_passes.emplace(new Pass(PassKind::Graphics, vull::move(name)));
}

ImageResource &RenderGraph::add_image(String name) {
    return static_cast<ImageResource &>(*m_resources.emplace(new ImageResource(vull::move(name))));
}

BufferResource &RenderGraph::add_storage_buffer(String name) {
    return static_cast<BufferResource &>(
        *m_resources.emplace(new BufferResource(ResourceKind::StorageBuffer, vull::move(name))));
}

BufferResource &RenderGraph::add_uniform_buffer(String name) {
    return static_cast<BufferResource &>(
        *m_resources.emplace(new BufferResource(ResourceKind::UniformBuffer, vull::move(name))));
}

void RenderGraph::compile(Resource &target) {
    // Post-order traversal to build a linear pass order.
    Function<void(Pass &)> dfs = [&](Pass &pass) {
        // Ignore already visited passes.
        if (pass.order_index() != ~0u) {
            return;
        }
        for (const auto &access : pass.reads()) {
            for (Pass &writer : access.resource.writers()) {
                dfs(writer);
            }
        }
        pass.set_order_index(m_pass_order.size());
        m_pass_order.push(pass);
    };

    // Perform the DFS starting from the writers of the target resource.
    for (Pass &writer : target.writers()) {
        dfs(writer);
    }

    // Insert barriers.
    for (const auto &resource : m_resources) {
        Vector<Pass &> ordered_users;
        ordered_users.extend(resource->readers());
        ordered_users.extend(resource->writers());
        vull::sort(ordered_users, [](const Pass &lhs, const Pass &rhs) {
            return lhs.order_index() > rhs.order_index();
        });

        auto current_layout = vkb::ImageLayout::Undefined;
        auto previous_access = vkb::Access2::None;
        auto previous_stage = vkb::PipelineStage2::None;
        for (uint32_t i = 0; i < ordered_users.size(); i++) {
            Pass &user = ordered_users[i];

            bool is_read = false;
            Optional<Optional<GenericBarrier> &> barrier_slot;
            for (auto &read : user.m_reads) {
                if (&read.resource == resource.ptr()) {
                    is_read = true;
                    barrier_slot = read.barrier;
                    break;
                }
            }
            if (!is_read) {
                for (auto &write : user.m_writes) {
                    if (&write.resource == resource.ptr()) {
                        barrier_slot = write.barrier;
                        break;
                    }
                }
            }

            // If we don't need either an execution barrier or a transition barrier (i.e. it's a buffer).
            if (previous_stage == vkb::PipelineStage2::None && resource->as_buffer()) {
                previous_stage = vull::get<0>(write_stage(user, *resource));
                continue;
            }

            GenericBarrier barrier{
                .src_stage = previous_stage,
                .src_access = previous_access,
            };
            if (const auto &image = resource->as_image()) {
                barrier.old_layout = current_layout;
                barrier.subresource_range = image->full_range();
            }

            if (!is_read) {
                auto [stage, access] = write_stage(user, *resource);
                barrier.dst_stage = (previous_stage = stage);
                barrier.dst_access = (previous_access = access);
                if (resource->kind() == ResourceKind::Image) {
                    barrier.new_layout = (current_layout = write_layout(user));
                }
                barrier_slot->emplace(barrier);
                continue;
            }

            for (uint32_t j = 0; j < i; j++) {
                Pass &writer = ordered_users[j];
                if (!writer.does_write_to(*resource)) {
                    continue;
                }
                VULL_ASSERT(writer.order_index() < user.order_index());
                auto [stage, access] = read_stage(user, *resource);
                barrier.src_access = vull::get<1>(write_stage(writer, *resource));
                barrier.dst_stage = stage;
                barrier.dst_access = access;
                if (resource->kind() == ResourceKind::Image) {
                    barrier.new_layout = (current_layout = vkb::ImageLayout::ReadOnlyOptimal);
                }
                barrier_slot->emplace(barrier);
            }
        }
    }
}

void RenderGraph::record(const CommandBuffer &cmd_buf, Optional<const QueryPool &> timestamp_pool) const {
    if (timestamp_pool) {
        cmd_buf.reset_query_pool(*timestamp_pool);
        cmd_buf.write_timestamp(vkb::PipelineStage2::None, *timestamp_pool, 0);
    }
    for (Pass &pass : m_pass_order) {
        pass.record(cmd_buf, timestamp_pool);
    }
    if (timestamp_pool) {
        cmd_buf.write_timestamp(vkb::PipelineStage2::AllCommands, *timestamp_pool, m_pass_order.size());
    }
}

template <typename T>
static auto node_id(const T &ref) {
    return reinterpret_cast<uintptr_t>(&ref);
}
template <typename T>
static auto node_id(const UniquePtr<T> &uptr) {
    return reinterpret_cast<uintptr_t>(uptr.ptr());
}

static String build_barrier_string(const GenericBarrier &barrier, bool is_image) {
    StringBuilder sb;
    sb.append("{} -&gt; {}<br></br>", stage_string(barrier.src_stage), stage_string(barrier.dst_stage));
    if (barrier.src_access != vkb::Access2::None || barrier.dst_access != vkb::Access2::None) {
        sb.append("{} -&gt; {}<br></br>", access_string(barrier.src_access), access_string(barrier.dst_access));
    }
    if (is_image && barrier.old_layout != barrier.new_layout) {
        sb.append("{} -&gt; {}<br></br>", layout_string(barrier.old_layout), layout_string(barrier.new_layout));
    }
    return sb.build();
}

String RenderGraph::to_dot() const {
    StringBuilder sb;
    sb.append("digraph {\n");
    sb.append("\trankdir = LR;\n");

    // Output input graph.
    for (const auto &resource : m_resources) {
        sb.append("\t\t{} [label=\"{}\", shape=oval, color={}];\n", node_id(*resource), resource->name(),
                  resource->kind() == ResourceKind::Image ? "hotpink" : "blue");
    }
    for (const auto &pass : m_passes) {
        sb.append("\t\t{} [label=\"{}\", shape=box, color=red];\n", node_id(pass), pass->name());
        for (const auto &[resource, barrier] : pass->reads()) {
            if (!barrier) {
                sb.append("\t\t{} -> {} [color=orange];\n", node_id(resource), node_id(pass));
            }
        }
        for (const auto &[resource, barrier] : pass->writes()) {
            if (!barrier) {
                sb.append("\t\t{} -> {} [color=fuchsia];\n", node_id(pass), node_id(resource));
            }
        }
    }

    // Output execution order.
    for (Optional<size_t> previous_node; const Pass &pass : m_pass_order) {
        StringBuilder read_label_sb;
        read_label_sb.append(R"(<table border="0" cellborder="1" cellspacing="0">)");
        bool has_any_read_barriers = false;
        for (bool first = true; const auto &[resource, barrier] : pass.reads()) {
            if (!barrier) {
                continue;
            }
            has_any_read_barriers = true;
            if (!vull::exchange(first, false)) {
                read_label_sb.append('\n');
            }
            read_label_sb.append("<tr><td PORT=\"{}\">", resource.name());
            read_label_sb.append("{}", build_barrier_string(*barrier, resource.as_image().has_value()));
            read_label_sb.append("</td></tr>");
        }
        read_label_sb.append("</table>");

        if (has_any_read_barriers && previous_node) {
            const auto barrier_id = node_id(pass.reads());
            sb.append("\t\t{} [label=<{}>, shape=octagon, color=orangered];\n", barrier_id, read_label_sb.build());
            sb.append("\t\t{} -> {} [color=black,penwidth=2];\n", *previous_node, barrier_id);
            for (const auto &[resource, barrier] : pass.reads()) {
                if (barrier) {
                    sb.append("\t\t{} -> {}:\"{}\" [color=orange];\n", node_id(resource), barrier_id, resource.name());
                    sb.append("\t\t{}:\"{}\" -> {} [color=orange];\n", barrier_id, resource.name(), node_id(pass));
                }
            }
            previous_node = barrier_id;
        }

        StringBuilder write_label_sb;
        write_label_sb.append(R"(<table border="0" cellborder="1" cellspacing="0">)");
        bool has_any_write_barriers = false;
        for (bool first = true; const auto &[resource, barrier] : pass.writes()) {
            if (!barrier) {
                continue;
            }
            has_any_write_barriers = true;
            if (!vull::exchange(first, false)) {
                write_label_sb.append('\n');
            }
            write_label_sb.append("<tr><td PORT=\"{}\">", resource.name());
            write_label_sb.append("{}", build_barrier_string(*barrier, resource.as_image().has_value()));
            write_label_sb.append("</td></tr>");
        }
        write_label_sb.append("</table>");

        if (has_any_write_barriers) {
            const auto barrier_id = node_id(pass.writes());
            sb.append("\t\t{} [label=<{}>, shape=octagon, color=orangered];\n", barrier_id, write_label_sb.build());
            if (previous_node) {
                sb.append("\t\t{} -> {} [color=black,penwidth=2];\n", *previous_node, barrier_id);
            }
            for (const auto &[resource, barrier] : pass.writes()) {
                if (barrier) {
                    sb.append("\t\t{} -> {}:\"{}\" [color=fuchsia];\n", node_id(pass), barrier_id, resource.name());
                    sb.append("\t\t{}:\"{}\" -> {} [color=fuchsia];\n", barrier_id, resource.name(), node_id(resource));
                }
            }
            previous_node = barrier_id;
        }

        if (previous_node) {
            sb.append("\t\t{} -> {} [color=black,penwidth=2];\n", *previous_node, node_id(pass));
        }
        previous_node = node_id(pass);
    }

    // Output key.
    sb.append(R"dot(
    subgraph cluster_key {
        label = "Key";
        node [shape=plaintext];

        k1a [label="Execution order", width=2];
        k1b [label="", style=invisible];
        k1a -> k1b [color=black, penwidth=2, headclip=false];

        k2a [label="Resource read", width=2];
        k2b [label="", style=invisible];
        k2a -> k2b [color=orange, headclip=false];

        k3a [label="Resource write", width=2];
        k3b [label="", style=invisible];
        k3a -> k3b [color=fuchsia, headclip=false];

        k4a [label="Pass"]
        k4b [label="", shape=box, color=red]

        k5a [label="Buffer"]
        k5b [label="", shape=oval, color=blue]

        k6a [label="Image"]
        k6b [label="", shape=oval, color=hotpink]

        k7a [label="Barrier"]
        k7b [label="", shape=octagon, color=orangered]

        { rank=source; k1a k2a k3a k4a k5a k6a k7a }
    })dot");

    sb.append("\n}\n");
    return sb.build();
}

} // namespace vull::vk
