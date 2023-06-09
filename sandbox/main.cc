#include "FreeCamera.hh"

#include <vull/container/Array.hh>
#include <vull/container/HashMap.hh>
#include <vull/container/HashSet.hh>
#include <vull/container/Vector.hh>
#include <vull/core/Input.hh>
#include <vull/core/Log.hh>
#include <vull/core/Main.hh>
#include <vull/core/Scene.hh>
#include <vull/core/Window.hh>
#include <vull/ecs/World.hh>
#include <vull/graphics/DefaultRenderer.hh>
#include <vull/graphics/DeferredRenderer.hh>
#include <vull/graphics/FramePacer.hh>
#include <vull/graphics/GBuffer.hh>
#include <vull/graphics/SkyboxRenderer.hh>
#include <vull/maths/Colour.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Projection.hh>
#include <vull/maths/Random.hh>
#include <vull/maths/Vec.hh>
#include <vull/physics/Collider.hh>
#include <vull/physics/PhysicsEngine.hh>
#include <vull/physics/RigidBody.hh>
#include <vull/platform/Timer.hh>
#include <vull/support/Algorithm.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Result.hh>
#include <vull/support/Span.hh>
#include <vull/support/StringView.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/tasklet/Tasklet.hh> // IWYU pragma: keep
#include <vull/ui/Font.hh>
#include <vull/ui/FontAtlas.hh>
#include <vull/ui/Painter.hh>
#include <vull/ui/Renderer.hh>
#include <vull/ui/Tree.hh>
#include <vull/ui/Window.hh>
#include <vull/ui/layout/Pane.hh>
#include <vull/ui/layout/ScreenPane.hh>
#include <vull/ui/widget/Button.hh>
#include <vull/ui/widget/TimeGraph.hh>
#include <vull/vpak/FileSystem.hh>
#include <vull/vpak/Reader.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Fence.hh>
#include <vull/vulkan/Queue.hh>
#include <vull/vulkan/RenderGraph.hh>
#include <vull/vulkan/Semaphore.hh>
#include <vull/vulkan/Swapchain.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

using namespace vull;

namespace vull::vk {

class CommandBuffer;

} // namespace vull::vk

void vull_main(Vector<StringView> &&args) {
    if (args.size() < 2) {
        vull::println("usage: {} [--enable-vvl] <scene-name>", args[0]);
        return;
    }

    StringView scene_name;
    bool enable_validation = false;
    for (const auto &arg : vull::slice(args, 1u)) {
        if (arg == "--enable-vvl") {
            enable_validation = true;
        } else if (arg[0] == '-') {
            vull::println("fatal: unknown option {}", arg);
            return;
        } else if (scene_name.empty()) {
            scene_name = arg;
        } else {
            vull::println("fatal: unexpected argument {}", arg);
            return;
        }
    }

    Window window({}, {}, true);
    vk::Context context(enable_validation);
    auto swapchain = window.create_swapchain(context, vk::SwapchainMode::LowPower);

    Scene scene(context);
    scene.load(scene_name);

    DeferredRenderer deferred_renderer(context, swapchain.extent_3D());
    DefaultRenderer default_renderer(context, swapchain.extent_3D());
    default_renderer.load_scene(scene);

    SkyboxRenderer skybox_renderer(context);
    if (auto stream = vpak::open("/skybox")) {
        skybox_renderer.load(*stream);
    }

    const auto projection = vull::infinite_perspective(window.aspect_ratio(), vull::half_pi<float>, 0.1f);

    ui::Renderer ui_renderer(context, window.ppcm());
    ui::Tree ui_tree(window.ppcm());

    auto font = VULL_EXPECT(ui::Font::load("/fonts/Inter-Medium", 18));
    ui::FontAtlas atlas(context, Vec2u(1024, 1024));

    auto &world = scene.world();
    world.register_component<RigidBody>();
    world.register_component<Collider>();

    FreeCamera free_camera;
    free_camera.set_position(50.0f);
    free_camera.set_pitch(-0.2f);
    free_camera.set_yaw(-2.0f);
    free_camera.handle_mouse_move({});

    bool mouse_visible = false;
    window.on_mouse_release(MouseButton::Middle, [&](Vec2f) {
        mouse_visible = !mouse_visible;
        mouse_visible ? window.show_cursor() : window.hide_cursor();
    });

    window.on_mouse_move([&](Vec2f delta, Vec2f position, MouseButtonMask buttons) {
        if (!window.cursor_hidden()) {
            ui_tree.handle_mouse_move(delta, position, buttons);
            return;
        }
        free_camera.handle_mouse_move(delta);
    });
    window.on_mouse_press(MouseButton::Left, [&](Vec2f) {
        if (!window.cursor_hidden()) {
            ui_tree.handle_mouse_press(MouseButton::Left);
        }
    });
    window.on_mouse_release(MouseButton::Left, [&](Vec2f) {
        if (!window.cursor_hidden()) {
            ui_tree.handle_mouse_release(MouseButton::Left);
        }
    });

    FramePacer frame_pacer(swapchain, 2);
    PhysicsEngine physics_engine;
    vull::seed_rand(5);

    // TODO: Don't require font to be passed everywhere.
    auto &screen_pane = ui_tree.set_root<ui::ScreenPane>();
    auto &main_window = screen_pane.add_child<ui::Window>("Main", font);
    main_window.set_offset_in_parent({2.0f, 2.0f});
    auto &cpu_time_graph = main_window.content_pane().add_child<ui::TimeGraph>(
        Vec2f(7.5f, 4.5f), Colour::from_rgb(0.4f, 0.6f, 0.5f), font, "CPU time");
    auto &gpu_time_graph = main_window.content_pane().add_child<ui::TimeGraph>(
        Vec2f(7.5f, 4.5f), Colour::from_rgb(0.8f, 0.5f, 0.7f), font, "GPU time");
    auto &quit_button = main_window.content_pane().add_child<ui::Button>(font, "Quit");
    quit_button.set_on_release([&] {
        window.close();
    });

    Timer frame_timer;
    cpu_time_graph.new_bar();
    while (!window.should_close()) {
        Timer acquire_frame_timer;
        auto &frame = frame_pacer.request_frame();
        cpu_time_graph.push_section("acquire-frame", acquire_frame_timer.elapsed());

        float dt = frame_timer.elapsed();
        frame_timer.reset();

        // Poll input.
        window.poll_events();

        // Collect previous frame N's timestamp data.
        const auto pass_times = frame.pass_times();
        gpu_time_graph.new_bar();
        for (const auto &[name, time] : pass_times) {
            if (name != "submit") {
                gpu_time_graph.push_section(name, time);
            }
        }

        // Step physics.
        Timer physics_timer;
        physics_engine.step(world, dt);
        cpu_time_graph.push_section("step-physics", physics_timer.elapsed());

        // Update camera.
        free_camera.update(window, dt);

        Timer ui_timer;
        auto ui_painter = ui_renderer.new_painter();
        ui_painter.bind_atlas(atlas);
        ui_tree.render(ui_painter);
        cpu_time_graph.new_bar();
        cpu_time_graph.push_section("render-ui", ui_timer.elapsed());

        default_renderer.set_cull_view_locked(window.is_key_pressed(Key::H));
        default_renderer.update_globals(projection, free_camera.view_matrix(), free_camera.position());

        Timer build_rg_timer;
        auto &cmd_buf = context.graphics_queue().request_cmd_buf();
        auto &graph = frame.new_graph(context);
        auto output_id = graph.import("output-image", swapchain.image(frame_pacer.image_index()));

        auto &gbuffer = deferred_renderer.create_gbuffer(graph);
        auto frame_ubo = default_renderer.build_pass(graph, gbuffer);
        output_id = deferred_renderer.build_pass(graph, frame_ubo, gbuffer, output_id);
        output_id = skybox_renderer.build_pass(graph, output_id, gbuffer.depth, frame_ubo);
        output_id = ui_renderer.build_pass(graph, output_id, vull::move(ui_painter));

        output_id = graph.add_pass<vk::ResourceId>(
            "submit", vk::PassFlags::None,
            [&](vk::PassBuilder &builder, vk::ResourceId &new_output) {
                new_output = builder.read(output_id, vk::ReadFlags::Present);
            },
            [&](vk::RenderGraph &graph, vk::CommandBuffer &cmd_buf, const vk::ResourceId &) {
                Array signal_semaphores{
                    vkb::SemaphoreSubmitInfo{
                        .sType = vkb::StructureType::SemaphoreSubmitInfo,
                        .semaphore = *frame.present_semaphore(),
                    },
                };
                Array wait_semaphores{
                    vkb::SemaphoreSubmitInfo{
                        .sType = vkb::StructureType::SemaphoreSubmitInfo,
                        .semaphore = *frame.acquire_semaphore(),
                        .stageMask = vkb::PipelineStage2::ColorAttachmentOutput,
                    },
                };
                auto &queue = graph.context().graphics_queue();
                queue.submit(cmd_buf, *frame.fence(), signal_semaphores.span(), wait_semaphores.span());
            });

        cpu_time_graph.push_section("build-rg", build_rg_timer.elapsed());

        Timer compile_rg_timer;
        graph.compile(output_id);
        cpu_time_graph.push_section("compile-rg", compile_rg_timer.elapsed());

        Timer execute_rg_timer;
        graph.execute(cmd_buf, true);
        cpu_time_graph.push_section("execute-rg", execute_rg_timer.elapsed());
    }
    context.vkDeviceWaitIdle();
}
