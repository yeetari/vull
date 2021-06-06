#include <vull/renderer/RenderGraph.hh>

#include <gtest/gtest.h>

namespace {

TEST(RenderGraphTest, Barrier) {
    RenderGraph graph;
    auto *back_buffer = graph.add<ImageResource>(ImageType::Normal, MemoryUsage::GpuOnly);
    auto *data_buffer = graph.add<BufferResource>(BufferType::StorageBuffer, MemoryUsage::HostVisible);
    auto *first_stage = graph.add<GraphicsStage>("first stage");
    auto *second_stage = graph.add<GraphicsStage>("second stage");
    first_stage->writes_to(data_buffer);
    second_stage->reads_from(data_buffer);
    second_stage->writes_to(back_buffer);

    auto compiled_graph = graph.compile(back_buffer);
    ASSERT_EQ(compiled_graph->barriers().size(), 1);
    EXPECT_EQ(compiled_graph->barriers()[0].src(), first_stage);
    EXPECT_EQ(compiled_graph->barriers()[0].dst(), second_stage);
    EXPECT_EQ(compiled_graph->barriers()[0].resource(), data_buffer);
    EXPECT_EQ(compiled_graph->semaphores().size(), 0);
    ASSERT_EQ(compiled_graph->stage_order().size(), 2);
    EXPECT_EQ(compiled_graph->stage_order()[0], first_stage);
    EXPECT_EQ(compiled_graph->stage_order()[1], second_stage);
}

TEST(RenderGraphTest, Complex) {
    RenderGraph graph;
    auto *back_buffer = graph.add<ImageResource>(ImageType::Normal, MemoryUsage::GpuOnly);
    auto *depth_buffer = graph.add<ImageResource>(ImageType::Depth, MemoryUsage::GpuOnly);
    auto *index_buffer = graph.add<BufferResource>(BufferType::IndexBuffer, MemoryUsage::HostVisible);
    auto *light_buffer = graph.add<BufferResource>(BufferType::StorageBuffer, MemoryUsage::HostVisible);
    auto *light_visibility_buffer = graph.add<BufferResource>(BufferType::StorageBuffer, MemoryUsage::HostVisible);
    auto *uniform_buffer = graph.add<BufferResource>(BufferType::UniformBuffer, MemoryUsage::HostVisible);
    auto *vertex_buffer = graph.add<BufferResource>(BufferType::VertexBuffer, MemoryUsage::HostVisible);
    auto *depth_pass = graph.add<GraphicsStage>("depth pass");
    auto *light_cull_pass = graph.add<ComputeStage>("light cull pass");
    auto *main_pass = graph.add<GraphicsStage>("main pass");
    depth_pass->reads_from(index_buffer);
    depth_pass->reads_from(uniform_buffer);
    depth_pass->reads_from(vertex_buffer);
    depth_pass->writes_to(depth_buffer);
    light_cull_pass->reads_from(depth_buffer);
    light_cull_pass->reads_from(light_buffer);
    light_cull_pass->reads_from(uniform_buffer);
    light_cull_pass->writes_to(light_visibility_buffer);
    main_pass->reads_from(depth_buffer);
    main_pass->reads_from(index_buffer);
    main_pass->reads_from(light_buffer);
    main_pass->reads_from(light_visibility_buffer);
    main_pass->reads_from(uniform_buffer);
    main_pass->reads_from(vertex_buffer);
    main_pass->writes_to(back_buffer);

    auto compiled_graph = graph.compile(back_buffer);
    ASSERT_EQ(compiled_graph->barriers().size(), 3);
    EXPECT_EQ(compiled_graph->barriers()[0].src(), depth_pass);
    EXPECT_EQ(compiled_graph->barriers()[0].dst(), light_cull_pass);
    EXPECT_EQ(compiled_graph->barriers()[0].resource(), depth_buffer);
    EXPECT_EQ(compiled_graph->barriers()[1].src(), depth_pass);
    EXPECT_EQ(compiled_graph->barriers()[1].dst(), main_pass);
    EXPECT_EQ(compiled_graph->barriers()[1].resource(), depth_buffer);
    EXPECT_EQ(compiled_graph->barriers()[2].src(), light_cull_pass);
    EXPECT_EQ(compiled_graph->barriers()[2].dst(), main_pass);
    EXPECT_EQ(compiled_graph->barriers()[2].resource(), light_visibility_buffer);
    EXPECT_EQ(compiled_graph->semaphores().size(), 0);
    ASSERT_EQ(compiled_graph->stage_order().size(), 3);
    EXPECT_EQ(compiled_graph->stage_order()[0], depth_pass);
    EXPECT_EQ(compiled_graph->stage_order()[1], light_cull_pass);
    EXPECT_EQ(compiled_graph->stage_order()[2], main_pass);
}

TEST(RenderGraphTest, MultipleBackBufferWriters) {
    RenderGraph graph;
    auto *back_buffer = graph.add<ImageResource>(ImageType::Normal, MemoryUsage::GpuOnly);
    auto *first_stage = graph.add<GraphicsStage>("first stage");
    auto *second_stage = graph.add<GraphicsStage>("second stage");
    first_stage->writes_to(back_buffer);
    second_stage->writes_to(back_buffer);

    auto compiled_graph = graph.compile(back_buffer);
    EXPECT_EQ(compiled_graph->barriers().size(), 0);
    ASSERT_EQ(compiled_graph->semaphores().size(), 1);
    EXPECT_EQ(compiled_graph->semaphores()[0].signaller(), first_stage);
    EXPECT_EQ(compiled_graph->semaphores()[0].waiter(), second_stage);
    ASSERT_EQ(compiled_graph->stage_order().size(), 2);
    EXPECT_EQ(compiled_graph->stage_order()[0], first_stage);
    EXPECT_EQ(compiled_graph->stage_order()[1], second_stage);
}

} // namespace
