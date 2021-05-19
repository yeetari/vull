#include <vull/renderer/RenderGraph.hh>

#include <gtest/gtest.h>

namespace {

TEST(RenderGraphTest, Barrier) {
    RenderGraph graph;
    auto *back_buffer = graph.add<RenderResource>("back buffer");
    auto *data_buffer = graph.add<RenderResource>("data buffer");
    auto *first_stage = graph.add<RenderStage>("first stage");
    auto *second_stage = graph.add<RenderStage>("second stage");
    first_stage->writes_to(data_buffer);
    second_stage->reads_from(data_buffer);
    second_stage->writes_to(back_buffer);

    auto compiled_graph = graph.compile(back_buffer);
    ASSERT_EQ(compiled_graph.barriers().size(), 1);
    EXPECT_EQ(compiled_graph.barriers()[0].src(), first_stage);
    EXPECT_EQ(compiled_graph.barriers()[0].dst(), second_stage);
    EXPECT_EQ(compiled_graph.barriers()[0].resource(), data_buffer);
    EXPECT_EQ(compiled_graph.semaphores().size(), 0);
    ASSERT_EQ(compiled_graph.stage_order().size(), 2);
    EXPECT_EQ(compiled_graph.stage_order()[0], first_stage);
    EXPECT_EQ(compiled_graph.stage_order()[1], second_stage);
}

TEST(RenderGraphTest, Complex) {
    RenderGraph graph;
    auto *back_buffer = graph.add<RenderResource>("back buffer");
    auto *depth_buffer = graph.add<RenderResource>("depth buffer");
    auto *index_buffer = graph.add<RenderResource>("index buffer");
    auto *light_buffer = graph.add<RenderResource>("light buffer");
    auto *light_visibility_buffer = graph.add<RenderResource>("light visibility buffer");
    auto *uniform_buffer = graph.add<RenderResource>("uniform buffer");
    auto *vertex_buffer = graph.add<RenderResource>("vertex buffer");
    auto *depth_pass = graph.add<RenderStage>("depth pass");
    auto *light_cull_pass = graph.add<RenderStage>("light cull pass");
    auto *main_pass = graph.add<RenderStage>("main pass");
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
    ASSERT_EQ(compiled_graph.barriers().size(), 3);
    EXPECT_EQ(compiled_graph.barriers()[0].src(), depth_pass);
    EXPECT_EQ(compiled_graph.barriers()[0].dst(), light_cull_pass);
    EXPECT_EQ(compiled_graph.barriers()[0].resource(), depth_buffer);
    EXPECT_EQ(compiled_graph.barriers()[1].src(), depth_pass);
    EXPECT_EQ(compiled_graph.barriers()[1].dst(), main_pass);
    EXPECT_EQ(compiled_graph.barriers()[1].resource(), depth_buffer);
    EXPECT_EQ(compiled_graph.barriers()[2].src(), light_cull_pass);
    EXPECT_EQ(compiled_graph.barriers()[2].dst(), main_pass);
    EXPECT_EQ(compiled_graph.barriers()[2].resource(), light_visibility_buffer);
    EXPECT_EQ(compiled_graph.semaphores().size(), 0);
    ASSERT_EQ(compiled_graph.stage_order().size(), 3);
    EXPECT_EQ(compiled_graph.stage_order()[0], depth_pass);
    EXPECT_EQ(compiled_graph.stage_order()[1], light_cull_pass);
    EXPECT_EQ(compiled_graph.stage_order()[2], main_pass);
}

TEST(RenderGraphTest, MultipleBackBufferWriters) {
    RenderGraph graph;
    auto *back_buffer = graph.add<RenderResource>("back buffer");
    auto *first_stage = graph.add<RenderStage>("first stage");
    auto *second_stage = graph.add<RenderStage>("second stage");
    first_stage->writes_to(back_buffer);
    second_stage->writes_to(back_buffer);

    auto compiled_graph = graph.compile(back_buffer);
    EXPECT_EQ(compiled_graph.barriers().size(), 0);
    ASSERT_EQ(compiled_graph.semaphores().size(), 1);
    EXPECT_EQ(compiled_graph.semaphores()[0].signaller(), first_stage);
    EXPECT_EQ(compiled_graph.semaphores()[0].waiter(), second_stage);
    ASSERT_EQ(compiled_graph.stage_order().size(), 2);
    EXPECT_EQ(compiled_graph.stage_order()[0], first_stage);
    EXPECT_EQ(compiled_graph.stage_order()[1], second_stage);
}

} // namespace
