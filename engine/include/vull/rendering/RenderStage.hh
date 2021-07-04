#pragma once

#include <vull/rendering/RenderNode.hh>
#include <vull/support/Vector.hh>

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

class Device;
class RenderResource;
class RenderTexture;
class Semaphore;
class Shader;

class RenderStage : public RenderNode {
protected:
    Vector<const RenderResource *> m_reads;
    Vector<const RenderResource *> m_writes;
    Vector<const Shader *> m_shaders;
    std::unordered_map<std::string, std::size_t> m_specialisation_constants;

    VkPipelineLayout m_pipeline_layout{nullptr};

    VkShaderStageFlags m_push_constant_stages{0};

    std::unordered_map<const RenderTexture *, VkImageLayout> m_initial_layouts;
    std::unordered_map<const RenderTexture *, VkImageLayout> m_final_layouts;

    // TODO: Not needed after pipeline created.
    Vector<VkSpecializationMapEntry> m_specialisation_map_entries;
    Vector<std::size_t> m_specialisation_values;
    VkSpecializationInfo m_specialisation_info{};

    Vector<Vector<VkSemaphore>> m_signal_semaphores;
    Vector<Vector<VkSemaphore>> m_wait_semaphores;
    Vector<VkPipelineStageFlags> m_wait_stages;

    const Device *m_device{nullptr};
    mutable VkCommandBuffer m_cmd_buf{nullptr};

public:
    explicit RenderStage(std::string &&name) : RenderNode(std::move(name)) {}
    RenderStage(const RenderStage &) = delete;
    RenderStage(RenderStage &&) = delete;
    ~RenderStage() override;

    RenderStage &operator=(const RenderStage &) = delete;
    RenderStage &operator=(RenderStage &&) = delete;

    void add_shader(const Shader &shader);
    void set_constant(std::string name, std::size_t value);
    void reads_from(RenderResource *resource);
    void writes_to(RenderResource *resource);

    void add_signal_semaphore(std::uint32_t frame_index, const Semaphore &semaphore);
    void add_wait_semaphore(std::uint32_t frame_index, const Semaphore &semaphore, VkPipelineStageFlags wait_stage);
    void set_initial_layout(const RenderTexture *texture, VkImageLayout layout);
    void set_final_layout(const RenderTexture *texture, VkImageLayout layout);

    virtual void build_objects(const Device &device, ExecutableGraph *executable_graph) override;
    virtual void start_recording(VkCommandBuffer cmd_buf) const;
    virtual VkCommandBuffer stop_recording() const;

    void push_constants(const void *data, std::uint32_t size);
    template <typename T>
    void push_constants(const T &data);

    const Vector<const RenderResource *> &reads() const { return m_reads; }
    const Vector<const RenderResource *> &writes() const { return m_writes; }
    const Vector<VkSemaphore> &signal_semaphores(std::uint32_t frame_index) const {
        return m_signal_semaphores[frame_index];
    }
    const Vector<VkSemaphore> &wait_semaphores(std::uint32_t frame_index) const {
        return m_wait_semaphores[frame_index];
    }
    const Vector<VkPipelineStageFlags> &wait_stages() const { return m_wait_stages; }
};

template <typename T>
void RenderStage::push_constants(const T &data) {
    push_constants(&data, sizeof(T));
}
