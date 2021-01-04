#define TINYOBJLOADER_IMPLEMENTATION
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <tiny_obj_loader.h>
#include <vull/Config.hh>
#include <vull/core/Transform.hh>
#include <vull/core/World.hh>
#include <vull/io/Window.hh>
#include <vull/renderer/Camera.hh>
#include <vull/renderer/Device.hh>
#include <vull/renderer/Instance.hh>
#include <vull/renderer/Mesh.hh>
#include <vull/renderer/RenderSystem.hh>
#include <vull/renderer/Surface.hh>
#include <vull/renderer/Swapchain.hh>
#include <vull/renderer/Vertex.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Log.hh>
#include <vull/support/Span.hh>
#include <vull/support/Vector.hh>

#include <algorithm>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>

namespace {

float g_prev_x = 0;
float g_prev_y = 0;

} // namespace

int main() {
    std::fstream config_file("config", std::ios::in);
    if (!config_file) {
        Log::info("sandbox", "Config file not found, creating default config");
        config_file.open("config", std::ios::out);
        config_file << "window_width: 800\n";
        config_file << "window_height: 600\n";
        config_file << "window_fullscreen: false\n";
        config_file << "# Choose between low_latency, low_power, normal and no_vsync.\n";
        config_file << "swapchain_mode: normal\n";
        config_file.flush();
        config_file.close();
        config_file.open("config", std::ios::in);
    }

    std::unordered_map<std::string, std::string> config;
    std::string line;
    while (std::getline(config_file, line)) {
        // Ignore comments.
        if (line.starts_with('#')) {
            continue;
        }
        const auto colon_position = line.find_first_of(':');
        auto key = line.substr(0, colon_position);
        auto val = line.substr(colon_position + 1);
        val.erase(std::remove_if(val.begin(), val.end(), isspace), val.end());
        config.emplace(std::move(key), std::move(val));
    }

    const int width = std::stoi(config.at("window_width"));
    const int height = std::stoi(config.at("window_height"));
    const bool fullscreen = config.at("window_fullscreen") == "true";
    const SwapchainMode swapchain_mode = [](const std::string &opt) -> SwapchainMode {
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
    }(config.at("swapchain_mode"));

    Window window(width, height, fullscreen);
    glfwSetInputMode(*window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    std::uint32_t required_extension_count = 0;
    const char **required_extensions = glfwGetRequiredInstanceExtensions(&required_extension_count);
    Instance instance({required_extensions, required_extension_count});
    Device device(instance, instance.physical_devices()[0]);
    Surface surface(instance, device, window);
    Swapchain swapchain(device, surface, swapchain_mode);

    Vector<Vertex> vertices;
    Vector<std::uint32_t> indices;
    std::unordered_map<Vertex, std::uint32_t> unique_vertices;
    auto load_obj = [&](const char *obj) {
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
    };
    load_obj("suzanne.obj");
    std::uint32_t suzanne_count = indices.size();
    load_obj("sponza.obj");
    std::uint32_t sponza_count = indices.size() - suzanne_count;

    World world;
    world.add<RenderSystem>(device, swapchain, window, vertices, indices);

    struct ScaleComponent {};
    class ScaleSystem : public System<ScaleSystem> {
        float m_time{0.0F};

    public:
        void update(World *world, float dt) override {
            m_time += dt;
            for (auto entity : world->view<ScaleComponent, Transform>()) {
                auto &transform = entity.get<Transform>()->matrix();
                transform[2][0] = std::abs(std::sin(m_time) * 8);
                transform[2][1] = std::abs(std::sin(m_time) * 8);
                transform[2][2] = std::abs(std::sin(m_time) * 8);
            }
        }
    };
    world.add<ScaleSystem>();

    struct SpinComponent {};
    struct SpinSystem : public System<SpinSystem> {
        void update(World *world, float dt) override {
            for (auto entity : world->view<SpinComponent, Transform>()) {
                auto &transform = entity.get<Transform>()->matrix();
                transform = glm::rotate(transform, dt * 10.0F, glm::vec3(0, 1, 0));
            }
        }
    };
    world.add<SpinSystem>();

    auto sponza = world.create_entity();
    sponza.add<Mesh>(sponza_count, suzanne_count);
    sponza.add<Transform>(glm::scale(glm::mat4(1.0F), glm::vec3(0.1F)));

    Vector<Entity> suzannes;
    suzannes.ensure_capacity(50);
    for (int i = 0; i < suzannes.capacity(); i++) {
        auto suzanne = world.create_entity();
        suzanne.add<Mesh>(suzanne_count, 0);
        suzanne.add<Transform>(
            glm::scale(glm::translate(glm::mat4(1.0F), glm::vec3(0.0F, i * 4 + 10, -5.0F)), glm::vec3(2.0F, 3.0F, 2.0F)));
        if (i % 2 == 0) {
            suzanne.add<ScaleComponent>();
        } else {
            suzanne.add<SpinComponent>();
        }
        suzannes.push(suzanne);
    }

    auto *renderer = world.get<RenderSystem>();
    auto &lights = renderer->lights();
    lights.resize(3000);
    Array<glm::vec3, 3000> dsts{};
    Array<glm::vec3, 3000> srcs{};
    for (int i = 0; auto &light : lights) {
        light.colour = glm::linearRand(glm::vec3(0.1F), glm::vec3(0.5F));
        light.radius = glm::linearRand(15.0F, 30.0F);
        light.position.x = glm::linearRand(-190, 175);
        light.position.y = glm::linearRand(-12, 138);
        light.position.z = glm::linearRand(-120, 103);
        dsts[i] = light.position;
        auto rand = glm::linearRand(30, 60);
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
    ubo.proj = glm::perspective(glm::radians(45.0F), window.aspect_ratio(), 0.1F, 1000.0F);
    ubo.proj[1][1] *= -1;

    Camera camera(glm::vec3(118, 18, -3), 0.6F, 1.25F);
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

        ubo.view = camera.view_matrix();
        ubo.camera_position = camera.position();
        camera.update(window, dt);
        for (int i = 0; auto &light : lights) {
            light.position = glm::mix(light.position, dsts[i], dt);
            if (glm::distance(light.position, dsts[i]) <= 6.0F) {
                std::swap(dsts[i], srcs[i]);
            }
            i++;
        }
        world.update(dt);
        Window::poll_events();
    }
}
