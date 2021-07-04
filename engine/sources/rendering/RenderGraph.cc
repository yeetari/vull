#include <vull/rendering/RenderGraph.hh>

#include <vull/rendering/ExecutableGraph.hh>
#include <vull/rendering/GraphicsStage.hh>
#include <vull/rendering/RenderResource.hh>
#include <vull/rendering/RenderStage.hh>
#include <vull/rendering/RenderTexture.hh>

#include <functional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

Box<CompiledGraph> RenderGraph::compile(const RenderResource *target) const {
    return Box<CompiledGraph>::create(this, target);
}

CompiledGraph::CompiledGraph(const RenderGraph *graph, const RenderResource *target) : m_graph(graph) {
    m_stage_order.ensure_capacity(graph->nodes().size());

    // Post order depth first search to build a linear order of stages.
    std::unordered_set<const RenderStage *> visited;
    std::function<void(RenderStage *)> dfs = [&](RenderStage *stage) {
        if (!visited.insert(stage).second) {
            return;
        }
        for (const auto *resource : stage->reads()) {
            for (auto *writer : resource->writers()) {
                dfs(writer);
            }
        }
        if (const auto *graphics_stage = stage->as<GraphicsStage>()) {
            for (const auto *resource : graphics_stage->inputs()) {
                for (auto *writer : resource->writers()) {
                    dfs(writer);
                }
            }
        }
        m_stage_order.push(stage);
    };

    // Perform the DFS starting from the writers of the target.
    for (auto *writer : target->writers()) {
        dfs(writer);
    }

    // Build image layout transitions.
    std::unordered_map<const RenderTexture *, VkImageLayout> image_layouts;
    for (auto *stage : m_stage_order) {
        auto handle_texture = [&](const RenderTexture *texture) {
            VkImageLayout initial_layout = image_layouts[texture];
            stage->set_initial_layout(texture, initial_layout);

            VkImageLayout final_layout = VK_IMAGE_LAYOUT_UNDEFINED;
            switch (texture->type()) {
            case TextureType::Depth:
                final_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                break;
            case TextureType::Normal:
                final_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                break;
            case TextureType::Swapchain:
                final_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                break;
            }
            stage->set_final_layout(texture, image_layouts[texture] = final_layout);
        };

        if (const auto *graphics_stage = stage->as<GraphicsStage>()) {
            for (const auto *input : graphics_stage->inputs()) {
                handle_texture(input);
            }
            for (const auto *output : graphics_stage->outputs()) {
                handle_texture(output);
            }
        }
    }
}

Box<ExecutableGraph> CompiledGraph::build_objects(const Device &device, std::uint32_t frame_queue_length) const {
    Box<ExecutableGraph> executable_graph(new ExecutableGraph(this, device, frame_queue_length));
    for (const auto &node : m_graph->nodes()) {
        node->build_objects(device, *executable_graph);
    }
    return executable_graph;
}

std::string CompiledGraph::to_dot() const {
    std::stringstream ss;
    ss << "digraph {\n";
    ss << "    rankdir = LR;\n";
    ss << "    node [shape=box];\n";

    std::size_t count = 0;
    std::size_t subgraph_count = 0;
    auto output_cluster = [&](const std::string &title, bool output_linearisation, bool, bool) {
        std::unordered_map<const void *, std::size_t> id_map;
        auto unique_id = [&](const void *obj) {
            if (!id_map.contains(obj)) {
                id_map.emplace(obj, count++);
            }
            return id_map.at(obj);
        };
        ss << "    subgraph cluster_" << std::to_string(subgraph_count++) << " {\n";
        ss << "        label = \"" << title << "\";\n";
        for (const auto &node : m_graph->nodes()) {
            if (const auto *resource = node->as<RenderResource>()) {
                ss << "        " << unique_id(resource) << " [label=\"" << resource->name() << "\", color=blue];\n";
            }
            if (const auto *stage = node->as<RenderStage>()) {
                ss << "        " << unique_id(stage) << " [label=\"" << stage->name() << "\", color=red];\n";
                for (const auto *resource : stage->writes()) {
                    ss << "        " << unique_id(stage) << " -> " << unique_id(resource) << " [color=orange];\n";
                }
                for (const auto *resource : stage->reads()) {
                    ss << "        " << unique_id(resource) << " -> " << unique_id(stage) << " [color=orange];\n";
                }
            }
            if (const auto *stage = node->as<GraphicsStage>()) {
                for (const auto *resource : stage->outputs()) {
                    ss << "        " << unique_id(stage) << " -> " << unique_id(resource) << " [color=deeppink];\n";
                }
                for (const auto *resource : stage->inputs()) {
                    ss << "        " << unique_id(resource) << " -> " << unique_id(stage) << " [color=deeppink];\n";
                }
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

        //        if (output_barriers) {
        //            for (const auto &barrier : m_barriers) {
        //                ss << "        " << unique_id(&barrier) << " [label=\"Barrier\"];\n";
        //                ss << "        " << unique_id(barrier.src()) << " -> " << unique_id(&barrier) << "
        //                [color=red];\n"; ss << "        " << unique_id(barrier.resource()) << " -> " <<
        //                unique_id(&barrier)
        //                   << " [color=orange];\n";
        //                ss << "        " << unique_id(&barrier) << " -> " << unique_id(barrier.dst()) << "
        //                [color=red];\n";
        //            }
        //        }
        //
        //        if (output_semaphores) {
        //            for (const auto &semaphore : m_semaphores) {
        //                ss << "        " << unique_id(semaphore.signaller()) << " -> " <<
        //                unique_id(semaphore.waiter())
        //                   << " [color=green4];\n";
        //            }
        //        }
        ss << "    }\n";
    };

    //    if (!m_semaphores.empty()) {
    //        output_cluster("Semaphore Insertion (" + std::to_string(m_semaphores.size()) + ")", true, true, true);
    //    }
    //    if (!m_barriers.empty()) {
    //        output_cluster("Barrier Insertion (" + std::to_string(m_barriers.size()) + ")", true, true, false);
    //    }
    output_cluster("Linearisation", true, false, false);
    output_cluster("Input", false, false, false);

    // Build key.
    ss << R"end(
    subgraph cluster_key {
        label = "Key";
        node [shape=plaintext];

        k1a [label="Barrier", width=2];
        k1b [style="invisible"];
        k1a -> k1b [color=red];

        k2a [label="Linear order", width=2];
        k2b [style="invisible"];
        k2a -> k2b [color=black,penwidth=2];

        k3a [label="Resource access (normal)", width=2];
        k3b [style="invisible"];
        k3a -> k3b [color=orange];

        k4a [label="Resource access (attachment)", width=2];
        k4b [style="invisible"];
        k4a -> k4b [color=deeppink];

        k5a [label="Semaphore", width=2];
        k5b [style="invisible"];
        k5a -> k5b [color=green4];
    })end";

    ss << "\n}\n";
    return ss.str();
}
