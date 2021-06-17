#pragma once

#include <vull/renderer/Swapchain.hh>
#include <vull/support/Box.hh>
#include <vull/support/Castable.hh>
#include <vull/support/Vector.hh>

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

class CompiledGraph;
class Device;
class ExecutableGraph;
class Fence;
class FrameData;
class RenderGraph;
class Semaphore;
class Shader;

enum class ResourceKind {
    Buffer,
    Image,
};

enum class MemoryUsage {
    GpuOnly,
    HostVisible,
    TransferOnce,
};

class RenderResource : public Castable<RenderResource, ResourceKind> {
    friend Castable<RenderResource, ResourceKind>;
    friend CompiledGraph;
    friend ExecutableGraph;
    friend FrameData;

private:
    const ResourceKind m_kind;
    const MemoryUsage m_usage;
    const std::uint32_t m_index;
    std::string m_name;

public:
    RenderResource(ResourceKind kind, MemoryUsage usage, std::uint32_t index)
        : m_kind(kind), m_usage(usage), m_index(index) {}
    RenderResource(const RenderResource &) = delete;
    RenderResource(RenderResource &&) = delete;
    virtual ~RenderResource() = default;

    RenderResource &operator=(const RenderResource &) = delete;
    RenderResource &operator=(RenderResource &&) = delete;

    void set_name(std::string name) { m_name = std::move(name); }
};

enum class BufferType {
    IndexBuffer,
    StorageBuffer,
    UniformBuffer,
    VertexBuffer,
};

class BufferResource : public RenderResource {
    friend CompiledGraph;
    friend ExecutableGraph;
    friend FrameData;

private:
    const BufferType m_type;
    VkDeviceSize m_initial_size{0};

    // TODO: These only apply to vertex buffers, maybe there should be a separate VertexBufferResource?
    Vector<VkVertexInputAttributeDescription> m_vertex_attributes;
    std::uint32_t m_element_size{0};

public:
    static constexpr auto k_kind = ResourceKind::Buffer;
    BufferResource(std::uint32_t index, BufferType type, MemoryUsage usage)
        : RenderResource(k_kind, usage, index), m_type(type) {
        switch (type) {
        case BufferType::IndexBuffer:
            set_name("index buffer");
            break;
        case BufferType::StorageBuffer:
            set_name("storage buffer");
            break;
        case BufferType::UniformBuffer:
            set_name("uniform buffer");
            break;
        case BufferType::VertexBuffer:
            set_name("vertex buffer");
            break;
        }
    }

    void set_initial_size(VkDeviceSize initial_size) { m_initial_size = initial_size; }

    // TODO: These only apply to vertex buffers, maybe there should be a separate VertexBufferResource?
    void add_vertex_attribute(VkFormat format, std::uint32_t offset);
    void set_element_size(std::uint32_t element_size) { m_element_size = element_size; }
};

enum class ImageType {
    Depth,
    Normal,
    Swapchain,
};

class ImageResource : public RenderResource {
    friend CompiledGraph;
    friend ExecutableGraph;
    friend FrameData;

private:
    const ImageType m_type;
    VkClearValue m_clear_value{};
    VkExtent3D m_extent{};
    VkFormat m_format{VK_FORMAT_UNDEFINED};

public:
    static constexpr auto k_kind = ResourceKind::Image;
    ImageResource(std::uint32_t index, ImageType type, MemoryUsage usage)
        : RenderResource(k_kind, usage, index), m_type(type) {
        if (type == ImageType::Depth) {
            set_name("depth buffer");
        }
    }

    void set_clear_value(VkClearValue clear_value) { m_clear_value = clear_value; }
    void set_extent(VkExtent3D extent) { m_extent = extent; }
    void set_format(VkFormat format) { m_format = format; }

    ImageType type() const { return m_type; }
};

class SwapchainResource : public ImageResource {
    friend CompiledGraph;

private:
    const Swapchain &m_swapchain;

public:
    SwapchainResource(std::uint32_t index, const Swapchain &swapchain)
        : ImageResource(index, ImageType::Swapchain, MemoryUsage::GpuOnly), m_swapchain(swapchain) {
        set_extent(swapchain.extent());
        set_format(swapchain.format());
        set_name("back buffer");
    }
};

enum class StageKind {
    Compute,
    Graphics,
};

class RenderStage : public Castable<RenderStage, StageKind> {
    friend Castable<RenderStage, StageKind>;
    friend CompiledGraph;
    friend ExecutableGraph;
    friend FrameData;
    friend RenderGraph;

private:
    const StageKind m_kind;
    const std::uint32_t m_index;
    const std::string m_name;

    Vector<const RenderResource *> m_reads;
    Vector<const RenderResource *> m_writes;
    Vector<VkPushConstantRange> m_push_constant_ranges;

    // TODO: Create a new object to pass to the record function that encapsulates the command buffer and stage info.
    std::function<void(VkCommandBuffer, VkPipelineLayout)> m_on_record;

public:
    RenderStage(StageKind kind, std::uint32_t index, std::string name)
        : m_kind(kind), m_index(index), m_name(std::move(name)) {}
    RenderStage(const RenderStage &) = delete;
    RenderStage(RenderStage &&) = delete;
    virtual ~RenderStage() = default;

    RenderStage &operator=(const RenderStage &) = delete;
    RenderStage &operator=(RenderStage &&) = delete;

    void reads_from(const RenderResource *resource) { m_reads.push(resource); }
    void writes_to(const RenderResource *resource) { m_writes.push(resource); }

    void add_push_constant_range(VkPushConstantRange range) { m_push_constant_ranges.push(range); }
    void set_on_record(std::function<void(VkCommandBuffer, VkPipelineLayout)> on_record) {
        m_on_record = std::move(on_record);
    }
};

class ComputeStage : public RenderStage {
    friend CompiledGraph;

private:
    VkPipelineShaderStageCreateInfo m_shader{};

public:
    static constexpr auto k_kind = StageKind::Compute;
    ComputeStage(std::uint32_t index, std::string &&name) : RenderStage(k_kind, index, name) {}

    void set_shader(const Shader &shader, const VkSpecializationInfo *specialisation_info = nullptr);
};

class GraphicsStage : public RenderStage {
    friend CompiledGraph;
    friend ExecutableGraph;
    friend RenderGraph;

private:
    Vector<const ImageResource *> m_inputs;
    Vector<const ImageResource *> m_outputs;
    VkPipelineShaderStageCreateInfo m_vertex_shader{};
    VkPipelineShaderStageCreateInfo m_fragment_shader{};

public:
    static constexpr auto k_kind = StageKind::Graphics;
    GraphicsStage(std::uint32_t index, std::string &&name) : RenderStage(k_kind, index, name) {}

    void add_input(const ImageResource *resource) { m_inputs.push(resource); }
    void add_output(const ImageResource *resource) { m_outputs.push(resource); }

    void set_vertex_shader(const Shader &shader, const VkSpecializationInfo *specialisation_info = nullptr);
    void set_fragment_shader(const Shader &shader, const VkSpecializationInfo *specialisation_info = nullptr);
};

class RenderGraph {
    Vector<Box<BufferResource>> m_buffers;
    Vector<Box<ImageResource>> m_images;
    Vector<Box<SwapchainResource>> m_swapchains;
    Vector<Box<ComputeStage>> m_compute_stages;
    Vector<Box<GraphicsStage>> m_graphics_stages;
    Vector<const RenderResource *> m_resources;

public:
    template <typename T, typename... Args>
    T *add(Args &&...args);

    Box<CompiledGraph> compile(const RenderResource *target) const;

    const Vector<Box<BufferResource>> &buffers() const { return m_buffers; }
    const Vector<Box<ImageResource>> &images() const { return m_images; }
    const Vector<Box<SwapchainResource>> &swapchains() const { return m_swapchains; }
    const Vector<Box<ComputeStage>> &compute_stages() const { return m_compute_stages; }
    const Vector<Box<GraphicsStage>> &graphics_stages() const { return m_graphics_stages; }
    const Vector<const RenderResource *> &resources() const { return m_resources; }
};

class CompiledGraph {
    friend RenderGraph;

private:
    const RenderGraph *const m_graph;
    Vector<const RenderStage *> m_stage_order;

    class Barrier {
        const RenderStage *m_src;
        const RenderStage *m_dst;
        const RenderResource *m_resource;

    public:
        Barrier(const RenderStage *src, const RenderStage *dst, const RenderResource *resource)
            : m_src(src), m_dst(dst), m_resource(resource) {}

        const RenderStage *src() const { return m_src; }
        const RenderStage *dst() const { return m_dst; }
        const RenderResource *resource() const { return m_resource; }
    };
    Vector<Barrier> m_barriers;

    class Semaphore {
        const RenderStage *m_signaller;
        const RenderStage *m_waiter;

    public:
        Semaphore(const RenderStage *signaller, const RenderStage *waiter) : m_signaller(signaller), m_waiter(waiter) {}

        const RenderStage *signaller() const { return m_signaller; }
        const RenderStage *waiter() const { return m_waiter; }
    };
    Vector<Semaphore> m_semaphores;

    CompiledGraph(const RenderGraph *graph) : m_graph(graph) {}

    static void build_compute_pipeline(const Device &, const ComputeStage *, VkPipelineLayout, VkPipeline *);
    static void build_graphics_pipeline(const Device &, const GraphicsStage *, VkPipelineLayout, VkRenderPass,
                                        VkPipeline *);

public:
    Box<ExecutableGraph> build_objects(const Device &device, std::uint32_t frame_queue_length);
    std::string to_dot() const;

    const Vector<Barrier> &barriers() const { return m_barriers; }
    const Vector<Semaphore> &semaphores() const { return m_semaphores; }
    const Vector<const RenderStage *> &stage_order() const { return m_stage_order; }
};

class FrameData {
    friend CompiledGraph;
    friend ExecutableGraph;

private:
    const Device &m_device;
    const RenderGraph *const m_graph;
    const CompiledGraph *const m_compiled_graph;
    const ExecutableGraph *const m_executable_graph;
    VkCommandPool m_command_pool{nullptr};
    VkCommandPool m_transfer_pool{nullptr};
    VkCommandBuffer m_transfer_buffer{nullptr};
    VkDescriptorPool m_descriptor_pool{nullptr};
    Vector<VkCommandBuffer> m_command_buffers;
    Vector<VkDescriptorSet> m_descriptor_sets;
    Vector<VkFramebuffer> m_framebuffers;

    // Physical resources.
    Vector<std::uint32_t> m_sizes;
    Vector<VkDeviceMemory> m_memories;
    Vector<VkBuffer> m_buffers;
    Vector<VkImage> m_images;
    Vector<VkImageView> m_image_views;
    Vector<VkSampler> m_samplers;

    // Staging resources.
    Vector<VkDeviceMemory> m_staging_memories;
    Vector<VkBuffer> m_staging_buffers;

    // Per frame transfers.
    struct Transfer {
        VkBuffer src;
        VkBuffer dst;
        VkDeviceSize size;
    };
    Vector<Transfer> m_transfer_queue;

    // Command synchronisation.
    struct PhysicalBarrier {
        VkPipelineStageFlags src{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
        VkPipelineStageFlags dst{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
        Vector<VkBufferMemoryBarrier> buffers;
        Vector<VkImageMemoryBarrier> images;
    };
    Vector<PhysicalBarrier> m_barriers;

    // Submit synchronisation.
    Vector<Vector<VkSemaphore>> m_signal_semaphores;
    Vector<Vector<VkSemaphore>> m_wait_semaphores;
    Vector<Vector<VkPipelineStageFlags>> m_wait_stages;

    void ensure_physical(const RenderResource *, VkDeviceSize);

public:
    FrameData(const Device &device, const RenderGraph *graph, const CompiledGraph *compiled_graph,
              const ExecutableGraph *executable_graph)
        : m_device(device), m_graph(graph), m_compiled_graph(compiled_graph), m_executable_graph(executable_graph) {}
    FrameData(const FrameData &) = delete;
    FrameData(FrameData &&other) noexcept
        : m_device(other.m_device), m_graph(other.m_graph), m_compiled_graph(other.m_compiled_graph),
          m_executable_graph(other.m_executable_graph), m_command_pool(std::exchange(other.m_command_pool, nullptr)),
          m_transfer_pool(std::exchange(other.m_transfer_pool, nullptr)),
          m_transfer_buffer(std::exchange(other.m_transfer_buffer, nullptr)),
          m_descriptor_pool(std::exchange(other.m_descriptor_pool, nullptr)),
          m_command_buffers(std::move(other.m_command_buffers)), m_descriptor_sets(std::move(other.m_descriptor_sets)),
          m_framebuffers(std::move(other.m_framebuffers)), m_sizes(std::move(other.m_sizes)),
          m_memories(std::move(other.m_memories)), m_buffers(std::move(other.m_buffers)),
          m_images(std::move(other.m_images)), m_image_views(std::move(other.m_image_views)),
          m_samplers(std::move(other.m_samplers)), m_staging_memories(std::move(other.m_staging_memories)),
          m_staging_buffers(std::move(other.m_staging_buffers)), m_transfer_queue(std::move(other.m_transfer_queue)),
          m_barriers(std::move(other.m_barriers)), m_signal_semaphores(std::move(other.m_signal_semaphores)),
          m_wait_semaphores(std::move(other.m_wait_semaphores)), m_wait_stages(std::move(other.m_wait_stages)) {}
    ~FrameData();

    FrameData &operator=(const FrameData &) = delete;
    FrameData &operator=(FrameData &&) = delete;

    void insert_signal_semaphore(const RenderStage *stage, const Semaphore &semaphore);
    void insert_wait_semaphore(const RenderStage *stage, const Semaphore &semaphore, VkPipelineStageFlags wait_stage);

    void transfer(const RenderResource *resource, const void *data, VkDeviceSize size);
    template <typename T>
    void transfer(const RenderResource *resource, const T &data);
    template <typename T, template <typename> typename Container>
    void transfer(const RenderResource *resource, const Container<T> &data);

    void upload(const RenderResource *resource, const void *data, VkDeviceSize size, VkDeviceSize offset = 0);
    template <typename T>
    void upload(const RenderResource *resource, const T &data, VkDeviceSize offset = 0);
    template <typename T, template <typename> typename Container>
    void upload(const RenderResource *resource, const Container<T> &data, VkDeviceSize offset = 0);
};

class ExecutableGraph {
    friend CompiledGraph;
    friend FrameData;

private:
    const Device &m_device;
    const RenderGraph *const m_graph;
    const CompiledGraph *const m_compiled_graph;
    Vector<FrameData> m_frame_datas;
    Vector<Vector<const ImageResource *>> m_image_orders;
    Vector<Vector<std::uint32_t>> m_resource_bindings;
    Vector<VkDescriptorSetLayout> m_descriptor_set_layouts;
    Vector<VkPipeline> m_pipelines;
    Vector<VkPipelineLayout> m_pipeline_layouts;
    Vector<VkRenderPass> m_render_passes;
    Vector<VkSubmitInfo> m_submit_infos;

    ExecutableGraph(const CompiledGraph *compiled_graph, const RenderGraph *graph, const Device &device,
                    std::uint32_t frame_queue_length)
        : m_device(device), m_graph(graph), m_compiled_graph(compiled_graph),
          m_frame_datas(frame_queue_length, device, graph, compiled_graph, this) {}

    void record_compute_commands(const ComputeStage *, FrameData &);
    void record_graphics_commands(const GraphicsStage *, FrameData &);

public:
    ExecutableGraph(const ExecutableGraph &) = delete;
    ExecutableGraph(ExecutableGraph &&) = delete;
    ~ExecutableGraph();

    ExecutableGraph &operator=(const ExecutableGraph &) = delete;
    ExecutableGraph &operator=(ExecutableGraph &&) = delete;

    void render(std::uint32_t frame_index, VkQueue queue, const Fence &signal_fence);
    FrameData &frame_data(std::uint32_t index);

    Vector<FrameData> &frame_datas() { return m_frame_datas; }
};

template <typename T, typename... Args>
T *RenderGraph::add(Args &&...args) {
    if constexpr (std::is_base_of_v<RenderResource, T>) {
        auto box = Box<T>::create(m_resources.size(), std::forward<Args>(args)...);
        m_resources.push(*box);
        if constexpr (std::is_same_v<T, BufferResource>) {
            return *m_buffers.emplace(std::move(box));
        } else if constexpr (std::is_same_v<T, ImageResource>) {
            return *m_images.emplace(std::move(box));
        } else if constexpr (std::is_same_v<T, SwapchainResource>) {
            return *m_swapchains.emplace(std::move(box));
        }
    } else if constexpr (std::is_same_v<T, ComputeStage>) {
        return *m_compute_stages.emplace(
            Box<T>::create(m_compute_stages.size() + m_graphics_stages.size(), std::forward<Args>(args)...));
    } else if constexpr (std::is_same_v<T, GraphicsStage>) {
        return *m_graphics_stages.emplace(
            Box<T>::create(m_compute_stages.size() + m_graphics_stages.size(), std::forward<Args>(args)...));
    } else {
        static_assert(!std::is_same_v<T, T>, "T must be either a RenderResource or a RenderStage");
    }
}

template <typename T>
void FrameData::transfer(const RenderResource *resource, const T &data) {
    transfer(resource, &data, sizeof(T));
}

template <typename T, template <typename> typename Container>
void FrameData::transfer(const RenderResource *resource, const Container<T> &data) {
    transfer(resource, data.data(), data.size_bytes());
}

template <typename T>
void FrameData::upload(const RenderResource *resource, const T &data, VkDeviceSize offset) {
    upload(resource, &data, sizeof(T), offset);
}

template <typename T, template <typename> typename Container>
void FrameData::upload(const RenderResource *resource, const Container<T> &data, VkDeviceSize offset) {
    upload(resource, data.data(), data.size_bytes(), offset);
}
