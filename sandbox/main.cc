#include "free_camera.hh"

#include <vull/container/array.hh>
#include <vull/container/hash_map.hh>
#include <vull/container/vector.hh>
#include <vull/core/application.hh>
#include <vull/core/input.hh>
#include <vull/ecs/world.hh>
#include <vull/graphics/default_renderer.hh>
#include <vull/graphics/deferred_renderer.hh>
#include <vull/graphics/frame_pacer.hh>
#include <vull/graphics/gbuffer.hh>
#include <vull/graphics/skybox_renderer.hh>
#include <vull/maths/colour.hh>
#include <vull/maths/common.hh>
#include <vull/maths/vec.hh>
#include <vull/physics/collider.hh>
#include <vull/physics/physics_engine.hh>
#include <vull/physics/rigid_body.hh>
#include <vull/platform/timer.hh>
#include <vull/platform/window.hh>
#include <vull/scene/scene.hh>
#include <vull/support/args_parser.hh>
#include <vull/support/function.hh>
#include <vull/support/optional.hh>
#include <vull/support/result.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/ui/element.hh>
#include <vull/ui/font.hh>
#include <vull/ui/font_atlas.hh>
#include <vull/ui/layout/pane.hh>
#include <vull/ui/layout/screen_pane.hh>
#include <vull/ui/painter.hh>
#include <vull/ui/renderer.hh>
#include <vull/ui/style.hh>
#include <vull/ui/tree.hh>
#include <vull/ui/widget/button.hh>
#include <vull/ui/widget/label.hh>
#include <vull/ui/widget/slider.hh>
#include <vull/ui/widget/time_graph.hh>
#include <vull/ui/window.hh>
#include <vull/vpak/file_system.hh>
#include <vull/vpak/stream.hh>
#include <vull/vulkan/command_buffer.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/fence.hh>
#include <vull/vulkan/query_pool.hh>
#include <vull/vulkan/queue.hh>
#include <vull/vulkan/render_graph.hh>
#include <vull/vulkan/semaphore.hh>
#include <vull/vulkan/swapchain.hh>
#include <vull/vulkan/vulkan.hh>

#include <stdint.h>

using namespace vull;

namespace {

class Sandbox {
    UniquePtr<platform::Window> m_window;
    vk::Context m_context;
    vk::Swapchain m_swapchain;
    FramePacer m_frame_pacer;
    vk::QueryPool m_pipeline_statistics_pool;
    DeferredRenderer m_deferred_renderer;
    DefaultRenderer m_default_renderer;
    SkyboxRenderer m_skybox_renderer;
    ui::Style m_ui_style;
    ui::Tree m_ui_tree;
    ui::Renderer m_ui_renderer;
    ui::FontAtlas m_font_atlas;

    ui::TimeGraph *m_cpu_time_graph;
    ui::TimeGraph *m_gpu_time_graph;
    ui::Slider *m_exposure_slider;
    ui::Slider *m_fov_slider;
    Vector<ui::Label &> m_pipeline_statistics_labels;

    FreeCamera m_free_camera;
    platform::Timer m_frame_timer;
    PhysicsEngine m_physics_engine;
    Scene m_scene;
    bool m_should_close{false};

public:
    Sandbox(bool enable_validation);

    void load_scene(StringView scene_name);
    void render_frame();
    void start_loop();
};

Sandbox::Sandbox(bool enable_validation)
    : m_window(VULL_EXPECT(platform::Window::create(vull::nullopt, vull::nullopt, true))), m_context(enable_validation),
      m_swapchain(VULL_EXPECT(m_window->create_swapchain(m_context, vk::SwapchainMode::LowPower))),
      m_frame_pacer(m_swapchain, 2),
      m_pipeline_statistics_pool(m_context, m_frame_pacer.queue_length(),
                                 vkb::QueryPipelineStatisticFlags::InputAssemblyVertices |
                                     vkb::QueryPipelineStatisticFlags::InputAssemblyPrimitives |
                                     vkb::QueryPipelineStatisticFlags::VertexShaderInvocations |
                                     vkb::QueryPipelineStatisticFlags::FragmentShaderInvocations |
                                     vkb::QueryPipelineStatisticFlags::ComputeShaderInvocations),
      m_deferred_renderer(m_context, m_swapchain.extent_3D()), m_default_renderer(m_context, m_swapchain.extent_3D()),
      m_skybox_renderer(m_context), m_ui_style(VULL_EXPECT(ui::Font::load("/fonts/Inter-Medium", 18)),
                                               VULL_EXPECT(ui::Font::load("/fonts/RobotoMono-Regular", 18))),
      m_ui_tree(m_ui_style, m_window->ppcm()), m_ui_renderer(m_context), m_font_atlas(m_context, Vec2u(512, 512)),
      m_free_camera(m_window->aspect_ratio()) {
    m_window->grab_cursor();
    m_window->on_close([this] {
        m_should_close = true;
    });
    m_window->on_mouse_release(MouseButton::Middle, [this](Vec2f) {
        m_window->cursor_grabbed() ? m_window->ungrab_cursor() : m_window->grab_cursor();
    });

    // TODO: Delta and position shouldn't be floats.
    m_window->on_mouse_move([this](Vec2f delta, Vec2f position, MouseButtonMask buttons) {
        if (!m_window->cursor_grabbed()) {
            m_ui_tree.handle_mouse_move(delta, position, buttons);
            return;
        }
        m_free_camera.handle_mouse_move(delta);
    });
    m_window->on_mouse_press(MouseButton::Left, [this](Vec2f) {
        if (!m_window->cursor_grabbed()) {
            m_ui_tree.handle_mouse_press(MouseButton::Left);
        }
    });
    m_window->on_mouse_release(MouseButton::Left, [this](Vec2f) {
        if (!m_window->cursor_grabbed()) {
            m_ui_tree.handle_mouse_release(MouseButton::Left);
        }
    });

    auto &screen_pane = m_ui_tree.set_root<ui::ScreenPane>();
    auto &main_window = screen_pane.add_child<ui::Window>("Main");
    main_window.content_pane().add_child<ui::Label>("F1 to show/hide");
    main_window.content_pane().add_child<ui::Label>("F2 for time graphs");
    main_window.content_pane().add_child<ui::Label>("F3 for pipeline statistics");
    main_window.content_pane().add_child<ui::Label>("F4 for camera settings");
    auto &quit_button = main_window.content_pane().add_child<ui::Button>("Quit");
    quit_button.set_on_release([this] {
        m_should_close = true;
    });

    auto &graphs_window = screen_pane.add_child<ui::Window>("Graphs");
    graphs_window.set_visible(false);
    m_cpu_time_graph =
        &graphs_window.content_pane().add_child<ui::TimeGraph>(Colour::from_rgb(0.4f, 0.6f, 0.5f), "CPU time");
    m_gpu_time_graph =
        &graphs_window.content_pane().add_child<ui::TimeGraph>(Colour::from_rgb(0.8f, 0.5f, 0.7f), "GPU time");

    auto &pipeline_statistics_window = screen_pane.add_child<ui::Window>("Pipeline statistics");
    pipeline_statistics_window.set_visible(false);
    for (uint32_t i = 0; i < 5; i++) {
        auto &label = pipeline_statistics_window.content_pane().add_child<ui::Label>();
        label.set_align(ui::Align::Right);
        label.set_font(m_ui_style.monospace_font());
        m_pipeline_statistics_labels.push(label);
    }

    auto &camera_window = screen_pane.add_child<ui::Window>("Camera settings");
    camera_window.set_visible(false);
    camera_window.content_pane().add_child<ui::Label>("Exposure");
    m_exposure_slider = &camera_window.content_pane().add_child<ui::Slider>(0.0f, 20.0f);
    camera_window.content_pane().add_child<ui::Label>("FOV");
    m_fov_slider = &camera_window.content_pane().add_child<ui::Slider>(0.0f, 180.0f);
    m_exposure_slider->set_value(5.0f);
    m_fov_slider->set_value(90.0f);

    m_window->on_key_release(Key::F1, [&](ModifierMask) {
        main_window.set_visible(!main_window.is_visible());
    });
    m_window->on_key_release(Key::F2, [&](ModifierMask) {
        graphs_window.set_visible(!graphs_window.is_visible());
    });
    m_window->on_key_release(Key::F3, [&](ModifierMask) {
        pipeline_statistics_window.set_visible(!pipeline_statistics_window.is_visible());
    });
    m_window->on_key_release(Key::F4, [&](ModifierMask) {
        camera_window.set_visible(!camera_window.is_visible());
    });

    m_cpu_time_graph->new_bar();
}

void Sandbox::load_scene(StringView scene_name) {
    m_scene.load(scene_name);
    m_default_renderer.load_scene(m_scene);
    if (auto skybox = vpak::open("/skybox")) {
        m_skybox_renderer.load(*skybox);
    }

    m_free_camera.set_position(50.0f);
    m_free_camera.set_pitch(-0.2f);
    m_free_camera.set_yaw(-2.0f);
    m_free_camera.handle_mouse_move({});

    auto &world = m_scene.world();
    world.register_component<RigidBody>();
    world.register_component<Collider>();
}

void Sandbox::render_frame() {
    platform::Timer acquire_frame_timer;
    auto &frame = m_frame_pacer.request_frame();
    m_cpu_time_graph->push_section("acquire-frame", acquire_frame_timer.elapsed());

    float dt = m_frame_timer.elapsed();
    m_frame_timer.reset();

    // Poll input.
    m_window->poll_events();

    // Collect previous frame N's timestamp data.
    const auto pass_times = frame.pass_times();
    m_gpu_time_graph->new_bar();
    for (const auto &[name, time] : pass_times) {
        if (name != "submit") {
            m_gpu_time_graph->push_section(name, time);
        }
    }

    // Collect pipeline statistics.
    constexpr Array pipeline_statistics_strings{
        "Assembled vertices: {d8 }", "Assembled primitives: {d8 }", "VS invocations: {d8 }",
        "FS invocations: {d8 }",     "CS invocations: {d8 }",
    };
    Array<uint64_t, 5> pipeline_statistics{};
    m_pipeline_statistics_pool.read_host(pipeline_statistics.span(), 1, m_frame_pacer.frame_index());
    for (uint32_t i = 0; i < pipeline_statistics.size(); i++) {
        static_cast<ui::Label &>(m_pipeline_statistics_labels[i])
            .set_text(vull::format(pipeline_statistics_strings[i], pipeline_statistics[i]));
    }

    // Step physics.
    platform::Timer physics_timer;
    m_physics_engine.step(m_scene.world(), dt);
    m_cpu_time_graph->push_section("step-physics", physics_timer.elapsed());

    // Update camera.
    m_free_camera.update(*m_window, dt);

    platform::Timer ui_timer;
    ui::Painter ui_painter;
    ui_painter.bind_atlas(m_font_atlas);
    m_ui_tree.render(ui_painter);
    m_cpu_time_graph->new_bar();
    m_cpu_time_graph->push_section("render-ui", ui_timer.elapsed());

    m_deferred_renderer.set_exposure(m_exposure_slider->value());
    m_default_renderer.set_cull_view_locked(m_window->is_key_pressed(Key::H));
    m_default_renderer.set_camera(m_free_camera);
    m_free_camera.set_fov(m_fov_slider->value() * (vull::pi<float> / 180.0f));

    platform::Timer build_rg_timer;
    auto &graph = frame.new_graph(m_context);
    auto output_id = graph.import("output-image", m_swapchain.image(m_frame_pacer.image_index()));

    auto gbuffer = m_deferred_renderer.create_gbuffer(graph);
    auto frame_ubo = m_default_renderer.build_pass(graph, gbuffer);
    m_deferred_renderer.build_pass(graph, gbuffer, frame_ubo, output_id);
    m_skybox_renderer.build_pass(graph, gbuffer.depth, frame_ubo, output_id);
    m_ui_renderer.build_pass(graph, output_id, vull::move(ui_painter));

    graph.add_pass("submit", vk::PassFlag::None).read(output_id, vk::ReadFlag::Present);
    m_cpu_time_graph->push_section("build-rg", build_rg_timer.elapsed());

    platform::Timer compile_rg_timer;
    graph.compile(output_id);
    m_cpu_time_graph->push_section("compile-rg", compile_rg_timer.elapsed());

    platform::Timer execute_rg_timer;
    auto queue = m_context.lock_queue(vk::QueueKind::Graphics);
    auto &cmd_buf = queue->request_cmd_buf();
    cmd_buf.reset_query(m_pipeline_statistics_pool, m_frame_pacer.frame_index());
    cmd_buf.begin_query(m_pipeline_statistics_pool, m_frame_pacer.frame_index());
    graph.execute(cmd_buf, true);
    cmd_buf.end_query(m_pipeline_statistics_pool, m_frame_pacer.frame_index());

    Array signal_semaphores{
        vkb::SemaphoreSubmitInfo{
            .sType = vkb::StructureType::SemaphoreSubmitInfo,
            .semaphore = *frame.present_semaphore(),
            .stageMask = vkb::PipelineStage2::AllCommands,
        },
    };
    Array wait_semaphores{
        vkb::SemaphoreSubmitInfo{
            .sType = vkb::StructureType::SemaphoreSubmitInfo,
            .semaphore = *frame.acquire_semaphore(),
            .stageMask = vkb::PipelineStage2::ColorAttachmentOutput,
        },
    };
    queue->submit(cmd_buf, *frame.fence(), signal_semaphores.span(), wait_semaphores.span());
    m_cpu_time_graph->push_section("execute-rg", execute_rg_timer.elapsed());
}

void Sandbox::start_loop() {
    while (!m_should_close) {
        render_frame();
    }
    m_context.vkDeviceWaitIdle();
}

} // namespace

int main(int argc, char **argv) {
    bool enable_validation = false;
    String scene_name;

    ArgsParser args_parser("vull-sandbox", "Vull Sandbox", "0.1.0");
    args_parser.add_flag(enable_validation, "Enable vulkan validation layer", "enable-vvl");
    args_parser.add_argument(scene_name, "scene-name", true);
    return vull::start_application(argc, argv, args_parser, [&] {
        Sandbox sandbox(enable_validation);
        sandbox.load_scene(scene_name);
        sandbox.start_loop();
    });
}
