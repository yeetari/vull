#include "Config.hh"
#include "VehicleController.hh"

#include <vull/core/Entity.hh>
#include <vull/core/Transform.hh>
#include <vull/core/World.hh>
#include <vull/io/FileSystem.hh>
#include <vull/io/PackFile.hh>
#include <vull/io/Window.hh>
#include <vull/physics/Collider.hh>
#include <vull/physics/PhysicsSystem.hh>
#include <vull/physics/RigidBody.hh>
#include <vull/physics/Vehicle.hh>
#include <vull/physics/shape/BoxShape.hh>
#include <vull/physics/shape/SphereShape.hh>
#include <vull/renderer/Camera.hh>
#include <vull/renderer/Device.hh>
#include <vull/renderer/Instance.hh>
#include <vull/renderer/Material.hh>
#include <vull/renderer/Mesh.hh>
#include <vull/renderer/PointLight.hh>
#include <vull/renderer/RenderSystem.hh>
#include <vull/renderer/Surface.hh>
#include <vull/renderer/Swapchain.hh>
#include <vull/renderer/UniformBuffer.hh>
#include <vull/support/Array.hh>
#include <vull/support/Log.hh>
#include <vull/support/Vector.hh>

#include <GLFW/glfw3.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/random.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>

namespace {

float g_prev_x = 0; // NOLINT
float g_prev_y = 0; // NOLINT

} // namespace

// NOLINTNEXTLINE
int main(int, char **argv) {
    FileSystem::initialise(argv[0]);
    Config config("config");
    config.parse();

    const SwapchainMode swapchain_mode = [](const std::string &opt) {
        if (opt == "low_latency") {
            return SwapchainMode::LowLatency;
        }
        if (opt == "low_power") {
            return SwapchainMode::LowPower;
        }
        if (opt == "normal") {
            return SwapchainMode::Normal;
        }
        if (opt == "no_vsync") {
            return SwapchainMode::NoVsync;
        }
        Log::error("sandbox", "Invalid swapchain mode %s in config", opt.c_str());
        std::exit(1);
    }(config.get<const std::string &>("swapchain_mode"));

    const auto width = config.get<std::uint32_t>("window_width");
    const auto height = config.get<std::uint32_t>("window_height");
    const auto fullscreen = config.get<bool>("window_fullscreen");
    Window window(width, height, fullscreen);
    glfwSetInputMode(*window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    std::uint32_t required_extension_count = 0;
    const char **required_extensions = glfwGetRequiredInstanceExtensions(&required_extension_count);
    Instance instance({required_extensions, required_extension_count});
    Device device(instance.physical_devices()[0]);
    Surface surface(instance, device, window);
    Swapchain swapchain(device, surface, swapchain_mode);

    auto vertices = FileSystem::load(PackEntryType::VertexBuffer, "sandbox");
    auto indices = FileSystem::load(PackEntryType::IndexBuffer, "sandbox");

    World world;
    world.add<PhysicsSystem>();
    world.add<VehicleSystem>();
    world.add<VehicleControllerSystem>(window);
    world.add<RenderSystem>(device, swapchain, vertices, indices);

    auto *renderer = world.get<RenderSystem>();
    auto cube_mesh = FileSystem::load_mesh("sandbox/meshes/cube");
    auto tire_mesh = FileSystem::load_mesh("sandbox/meshes/tire");
    auto pink_texture = renderer->upload_texture(FileSystem::load_texture("sandbox/textures/pink_checkerboard"));
    auto green_texture = renderer->upload_texture(FileSystem::load_texture("sandbox/textures/green_checkerboard"));
    auto marble_texture = renderer->upload_texture(FileSystem::load_texture("sandbox/textures/marble"));

    auto sponza = world.create_entity();
    sponza.add<Material>(green_texture);
    sponza.add<Mesh>(FileSystem::load_mesh("sandbox/meshes/sponza"));
    sponza.add<Transform>(glm::vec3(0.0f, -24.5f, 300.0f), glm::vec3(0.01f));

    BoxShape floor_shape(glm::vec3(1000.0f, 1.0f, 1000.0f));
    auto floor = world.create_entity();
    floor.add<Collider>(floor_shape);
    floor.add<RigidBody>(floor_shape, 0.0f, 0.0f);
    floor.add<Material>(marble_texture);
    floor.add<Mesh>(cube_mesh);
    floor.add<Transform>(glm::vec3(0.0f, -25.0f, 0.0f), floor_shape.half_size());

    auto &lights = renderer->lights();
    lights.resize(3000);
    Array<glm::vec3, 3000> dsts{};
    Array<glm::vec3, 3000> srcs{};
    for (int i = 0; auto &light : lights) {
        light.colour = glm::linearRand(glm::vec3(0.1f), glm::vec3(0.5f));
        light.radius = glm::linearRand(15.0f, 30.0f);
        light.position.x = glm::linearRand(-1000.0f, 1000.0f);
        light.position.y = glm::linearRand(-20.0f, 0.0f);
        light.position.z = glm::linearRand(-1000.0f, 1000.0f);
        dsts[i] = light.position;
        auto rand = glm::linearRand(30.0f, 60.0f);
        switch (glm::linearRand(0, 4)) {
        case 0:
            dsts[i].x += rand;
            break;
        case 1:
            dsts[i].y += rand;
            break;
        case 2:
            dsts[i].z += rand;
            break;
        case 3:
            dsts[i].x -= rand;
            break;
        case 4:
            dsts[i].z -= rand;
            break;
        }
        srcs[i++] = light.position;
    }

    auto &ubo = renderer->ubo();
    ubo.proj = glm::perspective(glm::radians(45.0f), window.aspect_ratio(), 0.1f, 1000.0f);
    ubo.proj[1][1] *= -1;

    BoxShape car_shape(glm::vec3(3.0f, 1.0f, 5.0f));
    auto car = world.create_entity();
    car.add<Collider>(car_shape);
    car.add<Material>(pink_texture);
    car.add<Mesh>(cube_mesh);
    car.add<RigidBody>(car_shape, 5.0_t, 0.1f);
    car.add<Transform>(glm::vec3(100.0f, -15.0f, 95.0f), car_shape.half_size());
    car.add<VehicleController>();

    auto *chassis = car.get<RigidBody>();
    chassis->set_angular_damping(0.5f);
    chassis->set_linear_damping(0.2f);

    auto *vehicle = car.add<Vehicle>();
    auto create_wheel = [&](Axle &axle, float radius, float x_offset, float roll) {
        auto visual_wheel = world.create_entity();
        visual_wheel.add<Material>(pink_texture);
        visual_wheel.add<Mesh>(tire_mesh);
        visual_wheel.add<Transform>(glm::vec3(0.0f));
        axle.add_wheel(radius, x_offset, visual_wheel.id()).set_roll(roll);
    };
    auto &front_axle = vehicle->add_axle(2.2f, 8.0f, 2.0f, 5.0f);
    create_wheel(front_axle, 1.1f, -3.5f, glm::radians(90.0f)); // FL
    create_wheel(front_axle, 1.1f, 3.5f, glm::radians(-90.0f)); // FR
    auto &rear_axle = vehicle->add_axle(2.2f, 8.0f, 2.0f, -5.0f);
    create_wheel(rear_axle, 1.1f, -3.5f, glm::radians(90.0f)); // RL
    create_wheel(rear_axle, 1.1f, 3.5f, glm::radians(-90.0f)); // RR

    BoxShape box_shape(glm::vec3(2.0f));
    for (int x = 0; x < 2; x++) {
        for (int z = 0; z < 2; z++) {
            for (int y = 0; y < 8; y++) {
                auto box = world.create_entity();
                box.add<Collider>(box_shape);
                box.add<Material>(pink_texture);
                box.add<Mesh>(cube_mesh);
                box.add<RigidBody>(box_shape, 10.0_kg, 0.0f);
                box.add<Transform>(glm::vec3(static_cast<float>(x) * 5.0f, static_cast<float>(y) * 4.5f - 15.0f,
                                             static_cast<float>(z) * 5.0f + 150.0f),
                                   box_shape.half_size());
            }
        }
    }

    Vector<BoxShape> stair_shapes;
    stair_shapes.ensure_capacity(30);
    for (std::size_t i = 0; i < stair_shapes.capacity(); i++) {
        stair_shapes.emplace(glm::vec3(10.0f, 0.15f * static_cast<float>(i + 1), 1.0f));
    }

    for (std::size_t i = 0; i < stair_shapes.capacity(); i++) {
        auto &shape = stair_shapes[i];
        auto stair = world.create_entity();
        stair.add<Collider>(shape);
        stair.add<Material>(green_texture);
        stair.add<Mesh>(cube_mesh);
        stair.add<RigidBody>(shape, 0.0f, 0.0f);
        stair.add<Transform>(
            glm::vec3(-100.0f, 0.15f * static_cast<float>(i + 1) - 24.0f, static_cast<float>(i) * 2.5f + 150.0f),
            shape.half_size());
    }

    BoxShape ramp_shape(glm::vec3(10.0f, 1.0f, 50.0f));
    auto ramp = world.create_entity();
    ramp.add<Collider>(ramp_shape);
    ramp.add<Material>(green_texture);
    ramp.add<Mesh>(cube_mesh);
    ramp.add<RigidBody>(ramp_shape, 0.0f, 0.0f);
    ramp.add<Transform>(glm::vec3(100.0f, -12.5f, 150.0f), ramp_shape.half_size())->orientation() =
        glm::angleAxis(glm::radians(-15.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    BoxShape line_shape(glm::vec3(5.0f, 0.6f, 1.0f));
    for (int i = 0; i < 10; i++) {
        auto line = world.create_entity();
        line.add<Collider>(line_shape);
        line.add<Material>(green_texture);
        line.add<Mesh>(cube_mesh);
        line.add<RigidBody>(line_shape, 0.0f, 0.0f);
        line.add<Transform>(glm::vec3(50.0f, -23.4f, static_cast<float>(i) * 8.0f + 200.0f), line_shape.half_size());
    }
    for (int i = 0; i < 10; i++) {
        auto line = world.create_entity();
        line.add<Collider>(line_shape);
        line.add<Material>(green_texture);
        line.add<Mesh>(cube_mesh);
        line.add<RigidBody>(line_shape, 0.0f, 0.0f);
        line.add<Transform>(glm::vec3(60.0f, -23.4f, static_cast<float>(i) * 8.0f + 204.0f), line_shape.half_size());
    }

    Camera camera(glm::vec3(0.0f));
    struct UserPtr {
        Camera *camera;
        Entity car;
        bool free_camera_active;
        bool fire;
    } user_ptr{
        .camera = &camera,
        .car = car,
        .free_camera_active = false,
        .fire = false,
    };
    glfwSetWindowUserPointer(*window, &user_ptr);
    glfwSetCursorPosCallback(*window, [](GLFWwindow *window, double xpos, double ypos) {
        auto *camera = static_cast<UserPtr *>(glfwGetWindowUserPointer(window))->camera;
        auto x = static_cast<float>(xpos);
        auto y = static_cast<float>(ypos);
        camera->handle_mouse_movement(x - g_prev_x, -(y - g_prev_y));
        g_prev_x = x;
        g_prev_y = y;
    });
    glfwSetKeyCallback(*window, [](GLFWwindow *window, int key, int, int action, int) {
        if (action != GLFW_RELEASE) {
            return;
        }
        auto *user_ptr = static_cast<UserPtr *>(glfwGetWindowUserPointer(window));
        if (key == GLFW_KEY_P) {
            auto &free_camera_active = user_ptr->free_camera_active;
            free_camera_active = !free_camera_active;
            if (free_camera_active) {
                const auto &position = user_ptr->car.get<Transform>()->position();
                const auto &orientation = user_ptr->car.get<Transform>()->orientation();
                user_ptr->camera->set_position(position + (orientation * glm::vec3(0.0f, 0.0f, -40.0f)) +
                                               glm::vec3(0.0f, 15.0f, 0.0f));
            }
        } else if (key == GLFW_KEY_Z) {
            user_ptr->fire = true;
        }
    });

    SphereShape sphere_shape(1.4f);
    double previous_time = glfwGetTime();
    double fps_counter_prev_time = glfwGetTime();
    int frame_count = 0;
    while (!window.should_close()) {
        double current_time = glfwGetTime();
        auto dt = static_cast<float>(current_time - previous_time);
        previous_time = current_time;
        frame_count++;
        if (current_time - fps_counter_prev_time >= 1.0) {
            Log::info("sandbox", "FPS: %d", frame_count);
            frame_count = 0;
            fps_counter_prev_time = current_time;
        }

        if (glfwGetMouseButton(*window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            dt /= 5;
        }
        if (glfwGetMouseButton(*window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            dt *= 5;
        }

        const auto &car_position = car.get<Transform>()->position();
        const auto &car_orientation = car.get<Transform>()->orientation();
        glm::vec3 camera_position =
            car_position + (car_orientation * glm::vec3(0.0f, 0.0f, -40.0f)) + glm::vec3(0.0f, 15.0f, 0.0f);
        glm::vec3 target_position = car_position + glm::vec3(0.0f, 10.0f, 0.0f);
        if (glfwGetKey(*window, GLFW_KEY_L) == GLFW_PRESS) {
            camera_position = glm::vec3(0.0f, 20.0f, 0.0f);
            target_position = glm::vec3(0.0f, -15.0f, 150.0f);
        }

        camera_position = glm::mix(ubo.camera_position, camera_position, dt * 3.0f);
        ubo.view = glm::lookAt(camera_position, target_position, glm::vec3(0.0f, 1.0f, 0.0f));
        ubo.camera_position = camera_position;

        // Override if free camera active.
        if (user_ptr.free_camera_active) {
            ubo.view = camera.view_matrix();
            ubo.camera_position = camera.position();
            camera.update(window, dt);
        }

        if (user_ptr.fire) {
            user_ptr.fire = false;
            auto bullet = world.create_entity();
            bullet.add<Collider>(sphere_shape);
            bullet.add<Material>(green_texture);
            bullet.add<Mesh>(FileSystem::load_mesh("sandbox/meshes/suzanne"));
            bullet.add<RigidBody>(sphere_shape, 100.0f, 0.1f);
            bullet.add<Transform>(user_ptr.free_camera_active ? ubo.camera_position : car_position);

            auto forward = user_ptr.free_camera_active ? camera.forward() : car.get<Transform>()->forward();
            bullet.get<RigidBody>()->apply_central_impulse(forward * 15000.0f);
        }

        for (int i = 0; auto &light : lights) {
            light.position = glm::mix(light.position, dsts[i], dt);
            if (glm::distance(light.position, dsts[i]) <= 6.0f) {
                std::swap(dsts[i], srcs[i]);
            }
            i++;
        }
        world.update(dt);
        Window::poll_events();
    }
    FileSystem::deinitialise();
}
