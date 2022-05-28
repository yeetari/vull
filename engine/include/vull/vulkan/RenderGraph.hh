#pragma once

#include <vull/support/Function.hh>
#include <vull/support/Optional.hh>
#include <vull/support/String.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class BufferResource;
class CommandBuffer;
class ImageResource;
class RenderGraph;
class Pass;

enum class ResourceKind {
    Image,
    StorageBuffer,
    UniformBuffer,
};

class Resource {
    friend Pass;

private:
    const ResourceKind m_kind;
    const String m_name;
    Vector<Pass &> m_readers;
    Vector<Pass &> m_writers;

public:
    Resource(ResourceKind kind, String &&name) : m_kind(kind), m_name(vull::move(name)) {}
    Resource(const Resource &) = delete;
    Resource(Resource &&) = delete;
    virtual ~Resource() = default;

    Resource &operator=(const Resource &) = delete;
    Resource &operator=(Resource &&) = delete;

    Optional<BufferResource &> as_buffer();
    Optional<ImageResource &> as_image();

    ResourceKind kind() const { return m_kind; }
    const String &name() const { return m_name; }
    const Vector<Pass &> &readers() const { return m_readers; }
    const Vector<Pass &> &writers() const { return m_writers; }
};

class BufferResource : public Resource {
    vkb::Buffer m_buffer{nullptr};

public:
    BufferResource(ResourceKind kind, String &&name) : Resource(kind, vull::move(name)) {}

    void set_buffer(vkb::Buffer buffer) { m_buffer = buffer; }
    operator vkb::Buffer() const { return m_buffer; }
};

class ImageResource : public Resource {
    vkb::Image m_image{nullptr};
    vkb::ImageView m_view{nullptr};
    vkb::ImageSubresourceRange m_full_range;

public:
    ImageResource(String &&name) : Resource(ResourceKind::Image, vull::move(name)) {}

    void set_image(vkb::Image image, vkb::ImageView view, const vkb::ImageSubresourceRange &full_range) {
        m_image = image;
        m_view = view;
        m_full_range = full_range;
    }

    operator vkb::Image() const { return m_image; }
    vkb::ImageView view() const { return m_view; }
    const vkb::ImageSubresourceRange &full_range() const { return m_full_range; }
};

struct GenericBarrier {
    vkb::PipelineStage2 src_stage;
    vkb::PipelineStage2 dst_stage;
    vkb::Access2 src_access;
    vkb::Access2 dst_access;
    union {
        struct {
            vkb::DeviceSize buffer_offset;
            vkb::DeviceSize buffer_size;
        };
        struct {
            vkb::ImageLayout old_layout;
            vkb::ImageLayout new_layout;
            vkb::ImageSubresourceRange subresource_range;
        };
    };

    vkb::BufferMemoryBarrier2 buffer_barrier(vkb::Buffer buffer) const;
    vkb::ImageMemoryBarrier2 image_barrier(vkb::Image image) const;
};

enum class PassKind {
    Compute,
    Graphics,
};

struct ResourceUse {
    Resource &resource;
    Optional<GenericBarrier> barrier;
};

class Pass {
    friend RenderGraph;

private:
    const PassKind m_kind;
    const String m_name;
    Vector<ResourceUse> m_reads;
    Vector<ResourceUse> m_writes;
    Function<void(const CommandBuffer &)> m_on_record;
    uint32_t m_order_index{~0u};

    bool does_write_to(Resource &resource);
    void record(const CommandBuffer &cmd_buf, vkb::QueryPool timestamp_pool);
    void set_order_index(uint32_t order_index) { m_order_index = order_index; }

public:
    Pass(PassKind kind, String &&name) : m_kind(kind), m_name(vull::move(name)) {}

    void reads_from(Resource &resource);
    void writes_to(Resource &resource);
    void set_on_record(Function<void(const CommandBuffer &)> on_record) { m_on_record = vull::move(on_record); }

    PassKind kind() const { return m_kind; }
    const String &name() const { return m_name; }
    const Vector<ResourceUse> &reads() const { return m_reads; }
    const Vector<ResourceUse> &writes() const { return m_writes; }
    uint32_t order_index() const { return m_order_index; }
};

class RenderGraph {
    Vector<UniquePtr<Pass>> m_passes;
    Vector<UniquePtr<Resource>> m_resources;
    Vector<Pass &> m_pass_order;

public:
    Pass &add_compute_pass(String name);
    Pass &add_graphics_pass(String name);
    ImageResource &add_image(String name);
    BufferResource &add_storage_buffer(String name);
    BufferResource &add_uniform_buffer(String name);
    void compile(Resource &target);
    void record(const CommandBuffer &cmd_buf, vkb::QueryPool timestamp_pool) const;
    String to_dot() const;

    const Vector<UniquePtr<Pass>> &passes() const { return m_passes; }
    const Vector<UniquePtr<Resource>> &resources() const { return m_resources; }
};

inline Optional<BufferResource &> Resource::as_buffer() {
    if (m_kind != ResourceKind::StorageBuffer && m_kind != ResourceKind::UniformBuffer) {
        return {};
    }
    return static_cast<BufferResource &>(*this);
}

inline Optional<ImageResource &> Resource::as_image() {
    return m_kind == ResourceKind::Image ? static_cast<ImageResource &>(*this) : Optional<ImageResource &>();
}

} // namespace vull::vk
