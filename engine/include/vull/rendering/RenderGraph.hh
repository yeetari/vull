#pragma once

#include <vull/rendering/RenderNode.hh>
#include <vull/support/Box.hh>
#include <vull/support/Vector.hh>

#include <cstdint>
#include <string>

class CompiledGraph;
class Device;
class ExecutableGraph;
class RenderResource;
class RenderStage;

class RenderGraph {
    Vector<Box<RenderNode>> m_nodes;

public:
    RenderGraph() = default;
    RenderGraph(const RenderGraph &) = delete;
    RenderGraph(RenderGraph &&) = delete;
    ~RenderGraph() = default;

    RenderGraph &operator=(const RenderGraph &) = delete;
    RenderGraph &operator=(RenderGraph &&) = delete;

    template <typename T, typename... Args>
    T *add(Args &&...args);
    Box<CompiledGraph> compile(const RenderResource *target) const;

    const Vector<Box<RenderNode>> &nodes() const { return m_nodes; }
};

class CompiledGraph {
    const RenderGraph *const m_graph;
    Vector<RenderStage *> m_stage_order;

public:
    CompiledGraph(const RenderGraph *graph, const RenderResource *target);

    // TODO: With the new system, this function is no longer safe to call multiple times.
    Box<ExecutableGraph> build_objects(const Device &device, std::uint32_t frame_queue_length = 1) const;
    std::string to_dot() const;

    const Vector<RenderStage *> &stage_order() const { return m_stage_order; }
};

template <typename T, typename... Args>
T *RenderGraph::add(Args &&...args) {
    return static_cast<T *>(*m_nodes.emplace(Box<T>::create(std::forward<Args>(args)...)));
}
