#pragma once

#include <vull/support/Box.hh>
#include <vull/support/Vector.hh>

#include <string>
#include <utility>

class CompiledGraph;
class RenderGraph;

class RenderResource {
    friend CompiledGraph;
    friend RenderGraph;

private:
    std::string m_name;

public:
    explicit RenderResource(std::string name) : m_name(std::move(name)) {}
};

class RenderStage {
    friend CompiledGraph;
    friend RenderGraph;

private:
    std::string m_name;
    Vector<const RenderResource *> m_reads;
    Vector<const RenderResource *> m_writes;

public:
    explicit RenderStage(std::string name) : m_name(std::move(name)) {}

    void reads_from(const RenderResource *resource);
    void writes_to(const RenderResource *resource);
};

class CompiledGraph {
    friend RenderGraph;

private:
    const RenderGraph *const m_graph;
    Vector<RenderStage *> m_stage_order;

    class Barrier {
        RenderStage *m_src;
        RenderStage *m_dst;
        const RenderResource *m_resource;

    public:
        Barrier(RenderStage *src, RenderStage *dst, const RenderResource *resource)
            : m_src(src), m_dst(dst), m_resource(resource) {}

        RenderStage *src() const { return m_src; }
        RenderStage *dst() const { return m_dst; }
        const RenderResource *resource() const { return m_resource; }
    };
    Vector<Barrier> m_barriers;

    class Semaphore {
        RenderStage *m_signaller;
        RenderStage *m_waiter;

    public:
        Semaphore(RenderStage *signaller, RenderStage *waiter) : m_signaller(signaller), m_waiter(waiter) {}

        RenderStage *signaller() const { return m_signaller; }
        RenderStage *waiter() const { return m_waiter; }
    };
    Vector<Semaphore> m_semaphores;

    explicit CompiledGraph(const RenderGraph *graph) : m_graph(graph) {}

public:
    std::string to_dot() const;

    const Vector<Barrier> &barriers() const { return m_barriers; }
    const Vector<Semaphore> &semaphores() const { return m_semaphores; }
    const Vector<RenderStage *> &stage_order() const { return m_stage_order; }
};

class RenderGraph {
    Vector<Box<RenderResource>> m_resources;
    Vector<Box<RenderStage>> m_stages;

public:
    template <typename T, typename... Args>
    T *add(Args &&...args);

    CompiledGraph compile(const RenderResource *target) const;

    const Vector<Box<RenderResource>> &resources() const { return m_resources; }
    const Vector<Box<RenderStage>> &stages() const { return m_stages; }
};

template <typename T, typename... Args>
T *RenderGraph::add(Args &&...args) {
    auto ptr = Box<T>::create(std::forward<Args>(args)...);
    if constexpr (std::is_same_v<T, RenderResource>) {
        return *m_resources.emplace(std::move(ptr));
    } else if constexpr (std::is_same_v<T, RenderStage>) {
        return *m_stages.emplace(std::move(ptr));
    } else {
        static_assert(!std::is_same_v<T, T>, "T must be either a RenderResource or a RenderStage");
    }
}
