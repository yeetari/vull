#include <vull/renderer/RenderGraph.hh>

#include <vull/support/Box.hh>
#include <vull/support/Vector.hh>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

void RenderStage::reads_from(const RenderResource *resource) {
    m_reads.push(resource);
}

void RenderStage::writes_to(const RenderResource *resource) {
    m_writes.push(resource);
}

CompiledGraph RenderGraph::compile(const RenderResource *target) const {
    CompiledGraph compiled_graph(this);
    auto &barriers = compiled_graph.m_barriers;
    auto &semaphores = compiled_graph.m_semaphores;
    auto &stage_order = compiled_graph.m_stage_order;

    std::unordered_map<const RenderResource *, Vector<RenderStage *>> writers;
    for (const auto &stage : m_stages) {
        for (const auto *resource : stage->m_writes) {
            writers[resource].push(*stage);
        }
    }

    // Post order depth first search to build a linear order.
    std::unordered_set<RenderStage *> visited;
    std::function<void(RenderStage *)> dfs = [&](RenderStage *stage) {
        if (!visited.insert(stage).second) {
            return;
        }
        for (const auto *resource : stage->m_reads) {
            for (auto *writer : writers[resource]) {
                dfs(writer);
            }
        }
        stage_order.push(stage);
    };

    // Perform the DFS starting from the writers of the target.
    for (auto *writer : writers[target]) {
        dfs(writer);
    }

    // Insert barriers for resources.
    for (const auto &stage : m_stages) {
        for (const auto *resource : stage->m_reads) {
            for (auto *writer : writers[resource]) {
                if (std::find(stage_order.begin(), stage_order.end(), writer) >
                    std::find(stage_order.begin(), stage_order.end(), *stage)) {
                    continue;
                }
                barriers.emplace(writer, *stage, resource);
            }
        }
    }

    // Insert semaphores between stages that write to the same resource.
    for (const auto &resource : m_resources) {
        for (RenderStage *wait_stage = nullptr; auto *writer : writers[*resource]) {
            if (wait_stage != nullptr) {
                semaphores.emplace(wait_stage, writer);
            }
            wait_stage = writer;
        }
    }
    return compiled_graph;
}

std::string CompiledGraph::to_dot() const {
    std::stringstream ss;
    ss << "digraph {\n";
    ss << "    rankdir = LR;\n";
    ss << "    node [shape=box];\n";

    std::size_t count = 0;
    std::size_t subgraph_count = 0;
    auto output_cluster = [&](const std::string &title, bool output_linearisation, bool output_barriers,
                              bool output_semaphores) {
        std::unordered_map<const void *, std::size_t> id_map;
        auto unique_id = [&](const void *obj) {
            if (!id_map.contains(obj)) {
                id_map.emplace(obj, count++);
            }
            return id_map.at(obj);
        };
        ss << "    subgraph cluster_" << std::to_string(subgraph_count++) << " {\n";
        ss << "        label = \"" << title << "\";\n";
        for (const auto &resource : m_graph->resources()) {
            ss << "        " << unique_id(*resource) << " [label=\"" << resource->m_name << "\", color=blue];\n";
        }

        for (const auto &stage : m_graph->stages()) {
            ss << "        " << unique_id(*stage) << " [label=\"" << stage->m_name << "\", color=red];\n";
            for (const auto *resource : stage->m_writes) {
                ss << "        " << unique_id(*stage) << " -> " << unique_id(resource) << " [color=orange];\n";
            }
            for (const auto *resource : stage->m_reads) {
                ss << "        " << unique_id(resource) << " -> " << unique_id(*stage) << " [color=orange];\n";
            }
        }

        if (output_linearisation) {
            for (const RenderStage *connect_stage = nullptr; const auto *stage : m_stage_order) {
                if (connect_stage != nullptr) {
                    ss << "        " << unique_id(connect_stage) << " -> " << unique_id(stage)
                       << " [color=black,penwidth=2];\n";
                }
                connect_stage = stage;
            }
        }

        if (output_barriers) {
            for (const auto &barrier : m_barriers) {
                ss << "        " << unique_id(&barrier) << " [label=\"Barrier\"];\n";
                ss << "        " << unique_id(barrier.src()) << " -> " << unique_id(&barrier) << " [color=red];\n";
                ss << "        " << unique_id(barrier.resource()) << " -> " << unique_id(&barrier)
                   << " [color=orange];\n";
                ss << "        " << unique_id(&barrier) << " -> " << unique_id(barrier.dst()) << " [color=red];\n";
            }
        }

        if (output_semaphores) {
            for (const auto &semaphore : m_semaphores) {
                ss << "        " << unique_id(semaphore.signaller()) << " -> " << unique_id(semaphore.waiter())
                   << " [color=green4];\n";
            }
        }
        ss << "    }\n";
    };

    output_cluster("Semaphore Insertion (" + std::to_string(m_semaphores.size()) + ")", true, true, true);
    output_cluster("Barrier Insertion (" + std::to_string(m_barriers.size()) + ")", true, true, false);
    output_cluster("Linearisation", true, false, false);
    output_cluster("Input", false, false, false);

    // Build key.
    ss << R"(
    subgraph cluster_key {
        label = "Key";
        node [shape=plaintext];

        k1a [label="Barrier", width=2];
        k1b [style="invisible"];
        k1a -> k1b [color=red];

        k2a [label="Linear order", width=2];
        k2b [style="invisible"];
        k2a -> k2b [color=black,penwidth=2];

        k3a [label="Resource access", width=2];
        k3b [style="invisible"];
        k3a -> k3b [color=orange];

        k4a [label="Semaphore", width=2];
        k4b [style="invisible"];
        k4a -> k4b [color=green4];
    })";

    ss << "\n}\n";
    return ss.str();
}
