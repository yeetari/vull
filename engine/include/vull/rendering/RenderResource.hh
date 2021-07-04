#pragma once

#include <vull/rendering/RenderNode.hh>
#include <vull/support/Vector.hh>

class Device;
class GraphicsStage;
class RenderStage;

class RenderResource : public RenderNode {
    friend GraphicsStage;
    friend RenderStage;

protected:
    const Device *m_device{nullptr}; // TODO: This is duplicated with RenderStage. Maybe move to render node because of
                                     //       that and also memory resource doesn't have access to device so freeing of
                                     //       memory has to be done in subclasses which isn't nice.
    ExecutableGraph *m_executable_graph{nullptr};
    Vector<RenderStage *> m_readers;
    Vector<RenderStage *> m_writers;

public:
    virtual void build_objects(const Device &device, ExecutableGraph *executable_graph) override {
        m_device = &device;
        m_executable_graph = executable_graph;
    }

    const Vector<RenderStage *> &readers() const { return m_readers; }
    const Vector<RenderStage *> &writers() const { return m_writers; }
};
