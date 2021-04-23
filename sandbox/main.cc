#include "Config.hh"

#include <vull/Config.hh>
#include <vull/core/Entity.hh>
#include <vull/core/System.hh>
#include <vull/core/Transform.hh>
#include <vull/core/World.hh>
#include <vull/io/Window.hh>
#include <vull/physics/Collider.hh>
#include <vull/physics/PhysicsSystem.hh>
#include <vull/physics/RigidBody.hh>
#include <vull/physics/shape/BoxShape.hh>
#include <vull/physics/shape/SphereShape.hh>
#include <vull/renderer/Camera.hh>
#include <vull/renderer/Device.hh>
#include <vull/renderer/Instance.hh>
#include <vull/renderer/Mesh.hh>
#include <vull/renderer/PointLight.hh>
#include <vull/renderer/RenderSystem.hh>
#include <vull/renderer/Surface.hh>
#include <vull/renderer/Swapchain.hh>
#include <vull/renderer/UniformBuffer.hh>
#include <vull/renderer/Vertex.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Log.hh>
#include <vull/support/Vector.hh>

#define TINYOBJLOADER_IMPLEMENTATION
#include <GLFW/glfw3.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <tiny_obj_loader.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

float g_prev_x = 0;
float g_prev_y = 0;

} // namespace

int main() {
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

    Vector<Vertex> vertices;
    Vector<std::uint32_t> indices;
    std::unordered_map<Vertex, std::uint32_t> unique_vertices;
    auto load_obj = [&](const char *obj) -> std::uint32_t {
        tinyobj::ObjReader reader;
        ENSURE(reader.ParseFromFile(std::string(k_model_path) + obj));
        std::uint32_t index_count = 0;
        for (const auto &shape : reader.GetShapes()) {
            index_count += shape.mesh.indices.size();
        }
        indices.ensure_capacity(index_count);
        const auto &attrib = reader.GetAttrib();
        for (const auto &shape : reader.GetShapes()) {
            for (const auto &index : shape.mesh.indices) {
                Vertex vertex{};
                vertex.position.x = attrib.vertices[3 * index.vertex_index + 0];
                vertex.position.y = attrib.vertices[3 * index.vertex_index + 1];
                vertex.position.z = attrib.vertices[3 * index.vertex_index + 2];
                vertex.normal.x = attrib.normals[3 * index.normal_index + 0];
                vertex.normal.y = attrib.normals[3 * index.normal_index + 1];
                vertex.normal.z = attrib.normals[3 * index.normal_index + 2];
                if (!unique_vertices.contains(vertex)) {
                    unique_vertices.emplace(vertex, vertices.size());
                    vertices.push(vertex);
                }
                indices.push(unique_vertices.at(vertex));
            }
        }
        return index_count;
    };
    std::uint32_t suzanne_count = load_obj("suzanne.obj");
    std::uint32_t sponza_count = load_obj("sponza.obj");
    std::uint32_t sphere_count = load_obj("sphere.obj");
    std::uint32_t cube_count = load_obj("cube.obj");

    World world;
    world.add<PhysicsSystem>();
    world.add<RenderSystem>(device, swapchain, window, vertices, indices);

    auto sponza = world.create_entity();
    sponza.add<Mesh>(sponza_count, suzanne_count);
    sponza.add<Transform>(glm::vec3(100.0f, -10.0f, 50.0f), glm::vec3(0.01f));

    auto floor = world.create_entity();
    floor.add<Collider>(*new BoxShape(glm::vec3(400.0f, 0.5f, 400.0f)));
    floor.add<Mesh>(cube_count, suzanne_count + sponza_count + sphere_count);
    floor.add<Transform>(glm::vec3(0.0f, -10.0f, 0.0f), glm::vec3(400.0f, 1.0f, 400.0f));

    auto *renderer = world.get<RenderSystem>();
    auto &lights = renderer->lights();
    lights.resize(3000);
    Array<glm::vec3, 3000> dsts{};
    Array<glm::vec3, 3000> srcs{};
    for (int i = 0; auto &light : lights) {
        light.colour = glm::linearRand(glm::vec3(0.1f), glm::vec3(0.5f));
        light.radius = glm::linearRand(15.0f, 30.0f);
        light.position.x = glm::linearRand(-400.0f, 400.0f);
        light.position.y = glm::linearRand(-6.0f, 6.0f);
        light.position.z = glm::linearRand(-400.0f, 400.0f);
        dsts[i] = light.position;
        auto rand = glm::linearRand(30.0f, 60.0f);
        switch (glm::linearRand(0, 5)) {
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
            dsts[i].y -= rand;
            break;
        case 5:
            dsts[i].z -= rand;
            break;
        }
        srcs[i++] = light.position;
    }

    auto &ubo = renderer->ubo();
    ubo.proj = glm::perspective(glm::radians(45.0f), window.aspect_ratio(), 0.1f, 1000.0f);
    ubo.proj[1][1] *= -1;

    Camera camera(glm::vec3(118, 18, -3), 0.6f, 1.25f);
    glfwSetWindowUserPointer(*window, &camera);
    glfwSetCursorPosCallback(*window, [](GLFWwindow *window, double xpos, double ypos) {
        auto *camera = static_cast<Camera *>(glfwGetWindowUserPointer(window));
        auto x = static_cast<float>(xpos);
        auto y = static_cast<float>(ypos);
        camera->handle_mouse_movement(x - g_prev_x, -(y - g_prev_y));
        g_prev_x = x;
        g_prev_y = y;
    });

    double previous_time = glfwGetTime();
    double fps_counter_prev_time = glfwGetTime();
    int frame_count = 0;
    int count = 0;
    float time = 0.0f;
    SphereShape sphere_shape(1.4f);
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

        if (time > 0.1f) {
            auto suzanne = world.create_entity();
            suzanne.add<Collider>(sphere_shape);
            suzanne.add<Mesh>(suzanne_count, 0);
            suzanne.add<RigidBody>(10.0f, 0.8f);
            suzanne.add<Transform>(glm::vec3(glm::sin(count) * 10.0f, count * 9 + 30, glm::cos(count) * 10.0f));
            time = 0.0f;
            count++;
        }
        count %= 50;
        time += dt;

        ubo.view = camera.view_matrix();
        ubo.camera_position = camera.position();
        camera.update(window, dt);
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
}
