#pragma once

#include <string>
#include <utility>

class Device;
class ExecutableGraph;

class RenderNode {
protected:
    std::string m_name;

public:
    RenderNode() = default;
    explicit RenderNode(std::string name) : m_name(std::move(name)) {}
    RenderNode(const RenderNode &) = delete;
    RenderNode(RenderNode &&) = delete;
    virtual ~RenderNode() = default;

    RenderNode &operator=(const RenderNode &) = delete;
    RenderNode &operator=(RenderNode &&) = delete;

    virtual void build_objects(const Device &device, ExecutableGraph *executable_graph) = 0;

    template <typename T>
    const T *as() const {
        return dynamic_cast<const T *>(this);
    }

    template <typename T>
    bool is() const {
        return as<T>() != nullptr;
    }

    void set_name(std::string name) { m_name = std::move(name); }

    const std::string &name() const { return m_name; }
};
