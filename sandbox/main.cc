#include "FreeCamera.hh"

#include <vull/core/Input.hh>
#include <vull/core/Material.hh>
#include <vull/core/Mesh.hh>
#include <vull/core/Scene.hh>
#include <vull/core/Transform.hh>
#include <vull/core/Window.hh>
#include <vull/ecs/Entity.hh>
#include <vull/ecs/EntityId.hh>
#include <vull/ecs/World.hh>
#include <vull/graphics/DefaultRenderer.hh>
#include <vull/graphics/Frame.hh>
#include <vull/graphics/FramePacer.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Mat.hh>
#include <vull/maths/Quat.hh>
#include <vull/maths/Random.hh>
#include <vull/maths/Vec.hh>
#include <vull/physics/Collider.hh>
#include <vull/physics/PhysicsEngine.hh>
#include <vull/physics/RigidBody.hh>
#include <vull/physics/Shape.hh>
#include <vull/platform/Timer.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Format.hh>
#include <vull/support/HashMap.hh>
#include <vull/support/HashSet.hh>
#include <vull/support/Result.hh>
#include <vull/support/Span.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Tuple.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/tasklet/Scheduler.hh>
#include <vull/tasklet/Tasklet.hh> // IWYU pragma: keep
#include <vull/ui/Renderer.hh>
#include <vull/ui/TimeGraph.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/CommandPool.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Fence.hh>
#include <vull/vulkan/Queue.hh>
#include <vull/vulkan/Semaphore.hh>
#include <vull/vulkan/Shader.hh>
#include <vull/vulkan/Swapchain.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace vull;

namespace {

uint32_t find_graphics_family(const vk::Context &context) {
    for (uint32_t i = 0; i < context.queue_families().size(); i++) {
        const auto &family = context.queue_families()[i];
        if ((family.queueFlags & vkb::QueueFlags::Graphics) != vkb::QueueFlags::None) {
            return i;
        }
    }
    VULL_ENSURE_NOT_REACHED();
}

Vector<uint8_t> load(const char *path) {
    FILE *file = fopen(path, "rb");
    fseek(file, 0, SEEK_END);
    Vector<uint8_t> binary(static_cast<uint32_t>(ftell(file)));
    fseek(file, 0, SEEK_SET);
    VULL_ENSURE(fread(binary.data(), 1, binary.size(), file) == binary.size());
    fclose(file);
    return binary;
}

void main_task(Scheduler &scheduler, StringView scene_name) {
    Window window(2560, 1440, true);
    vk::Context context;
    auto swapchain = window.create_swapchain(context, vk::SwapchainMode::LowPower);

    const auto graphics_family_index = find_graphics_family(context);
    vk::CommandPool cmd_pool(context, graphics_family_index);
    vk::Queue queue(context, graphics_family_index);

    Scene scene(context);
    scene.load(cmd_pool, queue, "scene.vpak", scene_name);

    auto default_vs = VULL_EXPECT(vk::Shader::parse(context, load("engine/shaders/default.vert.spv").span()));
    auto default_fs = VULL_EXPECT(vk::Shader::parse(context, load("engine/shaders/default.frag.spv").span()));
    auto deferred_shader = VULL_EXPECT(vk::Shader::parse(context, load("engine/shaders/deferred.comp.spv").span()));
    auto light_cull_shader = VULL_EXPECT(vk::Shader::parse(context, load("engine/shaders/light_cull.comp.spv").span()));
    auto shadow_shader = VULL_EXPECT(vk::Shader::parse(context, load("engine/shaders/shadow.vert.spv").span()));
    auto ui_vs = VULL_EXPECT(vk::Shader::parse(context, load("engine/shaders/ui.vert.spv").span()));
    auto ui_fs = VULL_EXPECT(vk::Shader::parse(context, load("engine/shaders/ui.frag.spv").span()));

    ShaderMap shader_map;
    shader_map.set("gbuffer-vert", vull::move(default_vs));
    shader_map.set("gbuffer-frag", vull::move(default_fs));
    shader_map.set("shadow", vull::move(shadow_shader));
    shader_map.set("light-cull", vull::move(light_cull_shader));
    shader_map.set("deferred", vull::move(deferred_shader));
    DefaultRenderer renderer(context, vull::move(shader_map), swapchain.extent_3D());
    renderer.load_scene(scene);

    const auto projection = vull::infinite_perspective(window.aspect_ratio(), vull::half_pi<float>, 0.1f);

    ui::Renderer ui(context, renderer.render_graph(), swapchain, renderer.output_image_resource(), ui_vs, ui_fs);
    ui::TimeGraph cpu_time_graph(Vec2f(600.0f, 300.0f), Vec3f(0.7f, 0.2f, 0.3f));
    ui::TimeGraph gpu_time_graph(Vec2f(600.0f, 300.0f), Vec3f(0.8f, 0.0f, 0.7f));
    auto font = ui.load_font("../engine/fonts/DejaVuSansMono.ttf", 20);
    ui.set_global_scale(window.ppcm() / 37.8f * 0.55f);

    renderer.compile_render_graph();

    auto &world = scene.world();
    world.register_component<RigidBody>();
    world.register_component<Collider>();

    for (auto [entity, mesh, transform] : world.view<Mesh, Transform>()) {
        if (strstr(mesh.vertex_data_name().data(), "Cube") == nullptr) {
            continue;
        }
        entity.add<Collider>(vull::make_unique<BoxShape>(transform.scale()));
    }

    auto player = world.create_entity();
    player.add<Transform>(~EntityId(0), Vec3f(0.0f, 10.0f, 0.0f), Quatf(), Vec3f(0.5f, 1.5f, 0.5f));
    player.add<Mesh>("/meshes/Cube.001.0/vertex", "/meshes/Cube.001.0/index");
    player.add<Material>("/default_albedo", "/default_normal");
    player.add<RigidBody>(250.0f);
    player.add<Collider>(vull::make_unique<BoxShape>(player.get<Transform>().scale()));
    player.get<RigidBody>().set_ignore_rotation(true);
    player.get<RigidBody>().set_shape(player.get<Collider>().shape());

    FreeCamera free_camera;
    bool free_camera_active = false;
    window.on_key_release(Key::F, [&](ModifierMask) {
        free_camera_active = !free_camera_active;
    });

    window.on_key_press(Key::Space, [&](ModifierMask) {
        float impulse = vull::sqrt(-2.0f * 6.0f * 250.0f * -9.81f * 100.0f);
        player.get<RigidBody>().apply_impulse({0.0f, impulse, 0.0f}, {});
    });

    bool mouse_visible = false;
    window.on_mouse_release(Button::Middle, [&](Vec2f) {
        mouse_visible = !mouse_visible;
        mouse_visible ? window.show_cursor() : window.hide_cursor();
    });

    float camera_pitch = 0.0f;
    float camera_yaw = 0.0f;
    window.on_mouse_move([&](Vec2f delta, Vec2f, ButtonMask) {
        if (free_camera_active) {
            free_camera.handle_mouse_move(delta);
            return;
        }
        camera_yaw -= delta.x() * (2.0f / static_cast<float>(window.width()));
        camera_pitch += delta.y() * (1.0f / static_cast<float>(window.height()));
        camera_pitch = vull::clamp(camera_pitch, -vull::half_pi<float> + 0.001f, vull::half_pi<float> - 0.001f);
        camera_yaw = vull::fmod(camera_yaw, vull::pi<float> * 2.0f);
    });

    FramePacer frame_pacer(swapchain, 2);
    PhysicsEngine physics_engine;
    vull::seed_rand(5);

    float fire_time = 0.0f;

    Timer frame_timer;
    cpu_time_graph.new_bar();
    while (!window.should_close()) {
        Timer acquire_frame_timer;
        auto &frame = frame_pacer.next_frame();
        cpu_time_graph.push_section("acquire-frame", acquire_frame_timer.elapsed());

        float dt = frame_timer.elapsed();
        frame_timer.reset();

        if (window.is_button_pressed(Button::Right)) {
            dt /= 5.0f;
        }

        // Poll input.
        window.poll_events();

        // Collect previous frame N's timestamp data.
        const auto pass_times = frame.pass_times(renderer.render_graph());
        gpu_time_graph.new_bar();
        for (const auto &[name, time] : pass_times) {
            gpu_time_graph.push_section(name, time);
        }

        // Step physics.
        Timer physics_timer;
        physics_engine.step(world, dt);
        cpu_time_graph.push_section("physics", physics_timer.elapsed());

        // Player friction force.
        auto &player_body = player.get<RigidBody>();
        player_body.apply_central_force(player_body.linear_velocity() * Vec3f(-1000.0f, 0.0f, -1000.0f));

        // Player and camera rotation.
        auto &player_transform = player.get<Transform>();
        Mat4f view_matrix;
        Vec3f view_position;
        Vec3f view_forward;
        if (!free_camera_active) {
            player_transform.set_rotation(vull::angle_axis(camera_yaw, Vec3f(0.0f, 1.0f, 0.0f)));
            view_forward =
                vull::rotate(player_transform.rotation() * vull::angle_axis(camera_pitch, Vec3f(1.0f, 0.0f, 0.0f)),
                             Vec3f(0.0f, 0.0f, 1.0f));
            view_position = player_transform.position() + Vec3f(0.0f, 1.5f, 0.0f);
            view_matrix = vull::look_at(view_position, view_position + view_forward, Vec3f(0.0f, 1.0f, 0.0f));
        } else {
            free_camera.update(window, dt);
            view_matrix = free_camera.view_matrix();
            view_position = free_camera.position();
            view_forward = free_camera.forward();
        }

        // Player movement.
        if (!free_camera_active) {
            const float speed = window.is_key_pressed(Key::Shift) ? 6250.0f : 1250.0f;
            if (window.is_key_pressed(Key::W)) {
                player_body.apply_central_force(player_transform.forward() * speed);
            }
            if (window.is_key_pressed(Key::S)) {
                player_body.apply_central_force(player_transform.forward() * -speed);
            }
            if (window.is_key_pressed(Key::A)) {
                player_body.apply_central_force(player_transform.right() * speed);
            }
            if (window.is_key_pressed(Key::D)) {
                player_body.apply_central_force(player_transform.right() * -speed);
            }
        }

        // Player shooting.
        if (window.is_button_pressed(Button::Left) && fire_time >= 0.1f) {
            constexpr float bullet_mass = 0.2f;
            const auto spawn_point = Vec3f(0.0f, 1.0f, 0.0f) + view_forward * 2.0f;
            auto box = world.create_entity();
            box.add<Transform>(~EntityId(0), player_transform.position() + spawn_point, Quatf(), Vec3f(0.2f));
            box.add<Mesh>("/meshes/Suzanne.0/vertex", "/meshes/Suzanne.0/index");
            box.add<Material>("/default_albedo", "/default_normal");
            box.add<Collider>(vull::make_unique<BoxShape>(Vec3f(0.2f)));
            box.add<RigidBody>(bullet_mass);
            box.get<RigidBody>().set_shape(box.get<Collider>().shape());
            box.get<RigidBody>().apply_impulse(view_forward * 5.0f, Vec3f(0.0f));
            box.get<RigidBody>().apply_impulse(player.get<RigidBody>().velocity_at_point(spawn_point) * bullet_mass,
                                               Vec3f(0.0f));
            fire_time = 0.0f;
        }
        fire_time += dt;

        // Destroy far away entities.
        for (auto [entity, body, transform] : world.view<RigidBody, Transform>()) {
            if (entity == player) {
                continue;
            }
            if (vull::distance(transform.position(), player.get<Transform>().position()) >= 100.0f) {
                entity.destroy();
            }
        }

        // Draw UI.
        ui.draw_rect(Vec4f(0.06f, 0.06f, 0.06f, 1.0f), {100.0f, 100.0f}, {1000.0f, 25.0f});
        ui.draw_rect(Vec4f(0.06f, 0.06f, 0.06f, 0.75f), {100.0f, 125.0f}, {1000.0f, 750.0f});
        cpu_time_graph.draw(ui, {120.0f, 200.0f}, font, "CPU time");
        gpu_time_graph.draw(ui, {120.0f, 550.0f}, font, "GPU time");
        ui.draw_text(font, {0.949f, 0.96f, 0.98f}, {95.0f, 140.0f},
                     vull::format("Camera position: ({}, {}, {}) {} {}", view_position.x(), view_position.y(),
                                  view_position.z(), camera_pitch, camera_yaw));

        const auto image_index = frame_pacer.image_index();
        vkb::Image swapchain_image = swapchain.image(image_index);
        vkb::ImageView swapchain_view = swapchain.image_view(image_index);

        Timer record_timer;
        auto &cmd_buf = cmd_pool.request_cmd_buf();
        renderer.render(cmd_buf, projection, view_matrix, view_position, swapchain_image, swapchain_view,
                        frame.timestamp_pool());

        vkb::ImageMemoryBarrier2 swapchain_present_barrier{
            .sType = vkb::StructureType::ImageMemoryBarrier2,
            .srcStageMask = vkb::PipelineStage2::ColorAttachmentOutput,
            .srcAccessMask = vkb::Access2::ColorAttachmentWrite,
            .oldLayout = vkb::ImageLayout::AttachmentOptimal,
            .newLayout = vkb::ImageLayout::PresentSrcKHR,
            .image = swapchain_image,
            .subresourceRange{
                .aspectMask = vkb::ImageAspect::Color,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        cmd_buf.image_barrier(swapchain_present_barrier);

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
        queue.submit(cmd_buf, *frame.fence(), signal_semaphores.span(), wait_semaphores.span());
        cpu_time_graph.new_bar();
        cpu_time_graph.push_section("record", record_timer.elapsed());
    }
    scheduler.stop();
    context.vkDeviceWaitIdle();
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <scene-name>\n", argv[0]);
        return EXIT_FAILURE;
    }

    Scheduler scheduler;
    scheduler.start([&] {
        main_task(scheduler, argv[1]);
    });
}
