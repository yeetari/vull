#pragma once

#include <vull/container/Vector.hh>
#include <vull/support/Enum.hh>
#include <vull/support/Function.hh>
#include <vull/support/String.hh>
#include <vull/support/Tuple.hh>
#include <vull/support/UniquePtr.hh> // IWYU pragma: keep
#include <vull/support/Utility.hh>
#include <vull/vulkan/QueryPool.hh>
#include <vull/vulkan/RenderGraphDefs.hh> // IWYU pragma: export
#include <vull/vulkan/Vulkan.hh>
// IWYU pragma: no_include "vull/vulkan/RenderGraphDefs.hh"

#include <stdint.h>

namespace vull::vk {

class Buffer;
class CommandBuffer;
class Context;
class Image;
class Pass;
class RenderGraph;

struct AttachmentDescription {
    vkb::Extent2D extent;
    vkb::Format format;
    vkb::ImageUsage usage;
    uint32_t mip_levels{1};
    uint32_t array_layers{1};
};

struct BufferDescription {
    vkb::DeviceSize size;
    vkb::BufferUsage usage;
    bool host_accessible{false};
};

enum class ResourceFlags {
    None = 0u,
    Buffer = 1u << 0u,
    Image = 1u << 1u,
    Imported = 1u << 2u,
    Uninitialised = 1u << 3u,
    Kind = Buffer | Image,
};

class PhysicalResource {
    String m_name;
    Function<const void *()> m_materialise;
    const void *m_materialised{nullptr};

public:
    PhysicalResource(String &&name, Function<const void *()> &&materialise)
        : m_name(vull::move(name)), m_materialise(vull::move(materialise)) {}

    const void *materialised();
    const String &name() const { return m_name; }
};

class Resource {
    Pass *m_producer{nullptr};
    ResourceFlags m_flags;
    vkb::PipelineStage2 m_write_stage{};
    vkb::Access2 m_write_access{};
    vkb::ImageLayout m_write_layout{};

public:
    Resource(Pass *producer, ResourceFlags flags) : m_producer(producer), m_flags(flags) {}

    void set_write_stage(vkb::PipelineStage2 stage) { m_write_stage = stage; }
    void set_write_access(vkb::Access2 access) { m_write_access = access; }
    void set_write_layout(vkb::ImageLayout layout) { m_write_layout = layout; }

    Pass &producer() const { return *m_producer; }
    ResourceFlags flags() const { return m_flags; }
    vkb::PipelineStage2 write_stage() const { return m_write_stage; }
    vkb::Access2 write_access() const { return m_write_access; }
    vkb::ImageLayout write_layout() const { return m_write_layout; }
};

enum class PassFlags {
    None = 0u,
    Compute = 1u << 0u,
    Graphics = 1u << 1u,
    Transfer = 1u << 2u,
    Kind = Compute | Graphics | Transfer,
};

enum class ReadFlags {
    None = 0u,

    /// Automatically applied when a write is specified as Additive. Used for render graph dependency tracking.
    Additive = 1u << 0u,

    /// Specifies that this read is via vkQueuePresent. This ensures that the image layout is correct. Only valid for an
    /// image resource.
    Present = 1u << 1u,

    /// Specifies that this read is via vkCmdDrawIndirect. Only valid for a buffer resource in a graphics pass.
    Indirect = 1u << 2u,

    /// Specifies that the image is sampled via a uniform rather than as an attachment. Only valid for an image resource
    /// in a graphics pass.
    Sampled = 1u << 3u,
};

enum class WriteFlags {
    None = 0u,

    /// Specifies that this write doesn't overwrite the resource. Ensures that previous writer(s) aren't culled, and
    /// uses vkb::AttachmentLoadOp::Load rather than DontCare or Clear.
    Additive = 1u << 0u,
};

class Pass {
    friend class PassBuilder;
    friend RenderGraph;

    struct Transition {
        ResourceId id;
        vkb::ImageLayout old_layout;
        vkb::ImageLayout new_layout;
    };

private:
    const String m_name;
    const PassFlags m_flags;
    RenderGraph &m_graph;
    Vector<Tuple<ResourceId, ReadFlags>> m_reads;
    Vector<Tuple<ResourceId, WriteFlags>> m_writes;
    Function<void(CommandBuffer &)> m_on_execute;

    vkb::MemoryBarrier2 m_memory_barrier{.sType = vkb::StructureType::MemoryBarrier2};
    Vector<Transition> m_transitions;
    bool m_visited{false};

    void add_transition(ResourceId id, vkb::ImageLayout old_layout, vkb::ImageLayout new_layout);
    vkb::DependencyInfo dependency_info(RenderGraph &graph, Vector<vkb::ImageMemoryBarrier2> &image_barriers) const;

public:
    Pass(RenderGraph &graph, String &&name, PassFlags flags)
        : m_name(vull::move(name)), m_flags(flags), m_graph(graph) {}
    Pass(const Pass &) = delete;
    Pass(Pass &&) = delete;
    virtual ~Pass() = default;

    Pass &operator=(const Pass &) = delete;
    Pass &operator=(Pass &&) = delete;

    Pass &read(ResourceId &id, ReadFlags flags = ReadFlags::None);
    Pass &write(ResourceId &id, WriteFlags flags = WriteFlags::None);
    void set_on_execute(Function<void(CommandBuffer &)> &&on_execute) { m_on_execute = vull::move(on_execute); }

    const String &name() const { return m_name; }
    PassFlags flags() const { return m_flags; }
    const Vector<Tuple<ResourceId, ReadFlags>> &reads() const { return m_reads; }
    const Vector<Tuple<ResourceId, WriteFlags>> &writes() const { return m_writes; }
};

class RenderGraph {
    friend Pass;

private:
    Context &m_context;
    Vector<UniquePtr<Pass>> m_passes;
    Vector<Pass &> m_pass_order;
    Vector<Resource, uint16_t> m_resources;
    Vector<PhysicalResource, uint16_t> m_physical_resources;
    vk::QueryPool m_timestamp_pool;

    Resource &virtual_resource(ResourceId id) { return m_resources[id.virtual_index()]; }
    PhysicalResource &physical_resource(ResourceId id) { return m_physical_resources[id.physical_index()]; }

    ResourceId create_resource(String &&name, ResourceFlags flags, Function<const void *()> &&materialise);
    ResourceId clone_resource(ResourceId id, Pass &producer);
    void build_order(ResourceId target);
    void build_sync();
    void record_pass(CommandBuffer &cmd_buf, Pass &pass);

public:
    explicit RenderGraph(Context &context);
    RenderGraph(const RenderGraph &) = delete;
    RenderGraph(RenderGraph &&) = delete;
    ~RenderGraph();

    RenderGraph &operator=(const RenderGraph &) = delete;
    RenderGraph &operator=(RenderGraph &&) = delete;

    Pass &add_pass(String name, PassFlags flags);
    ResourceId import(String name, const Buffer &buffer);
    ResourceId import(String name, const Image &image);
    ResourceId new_attachment(String name, const AttachmentDescription &description);
    ResourceId new_buffer(String name, const BufferDescription &description);

    const Buffer &get_buffer(ResourceId id);
    const Image &get_image(ResourceId id);

    void compile(ResourceId target);
    void execute(CommandBuffer &cmd_buf, bool record_timestamps);
    String to_json() const;

    Context &context() const { return m_context; }
    uint32_t pass_count() const { return m_pass_order.size(); }
    const Vector<Pass &> &pass_order() const { return m_pass_order; }
    vk::QueryPool &timestamp_pool() { return m_timestamp_pool; }
};

VULL_DEFINE_FLAG_ENUM_OPS(ResourceFlags)
VULL_DEFINE_FLAG_ENUM_OPS(PassFlags)
VULL_DEFINE_FLAG_ENUM_OPS(ReadFlags)
VULL_DEFINE_FLAG_ENUM_OPS(WriteFlags)

} // namespace vull::vk
