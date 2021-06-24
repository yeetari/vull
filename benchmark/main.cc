#include <vull/core/Entity.hh>
#include <vull/core/PointFollower.hh>
#include <vull/core/Transform.hh>
#include <vull/core/World.hh>
#include <vull/io/FileSystem.hh>
#include <vull/io/Window.hh>
#include <vull/maths/BezierCurve.hh>
#include <vull/renderer/Device.hh>
#include <vull/renderer/Instance.hh>
#include <vull/renderer/Material.hh>
#include <vull/renderer/Mesh.hh>
#include <vull/renderer/PointLight.hh>
#include <vull/renderer/RenderSystem.hh>
#include <vull/renderer/Surface.hh>
#include <vull/renderer/Swapchain.hh>
#include <vull/renderer/UniformBuffer.hh>
#include <vull/renderer/Vertex.hh>
#include <vull/support/Array.hh>
#include <vull/support/Vector.hh>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/random.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <tracy/Tracy.hpp>
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <fstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

void *operator new(std::size_t count) {
    auto *ptr = malloc(count);
    TracyAlloc(ptr, count);
    return ptr;
}

void operator delete(void *ptr) noexcept {
    TracyFree(ptr);
    free(ptr);
}

int main() {
    FileSystem::initialise("./sandbox/vull-sandbox");
    glfwInit();
    const auto *video_mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    Window window(video_mode->width, video_mode->height, true);
    glfwSetInputMode(*window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    std::uint32_t required_extension_count = 0;
    const char **required_extensions = glfwGetRequiredInstanceExtensions(&required_extension_count);
    Instance instance({required_extensions, required_extension_count});
    Device device(instance.physical_devices()[0]);
    Surface surface(instance, device, window);
    Swapchain swapchain(device, surface, SwapchainMode::LowLatency);

    auto vertices = FileSystem::load(PackEntryType::VertexBuffer, "sandbox");
    auto indices = FileSystem::load(PackEntryType::IndexBuffer, "sandbox");

    World world;
    world.add<PointFollowerSystem>();
    world.add<RenderSystem>(device, swapchain, vertices, indices);

    auto *renderer = world.get<RenderSystem>();
    auto cube_mesh = FileSystem::load_mesh("sandbox/meshes/cube");
    auto sponza_mesh = FileSystem::load_mesh("sandbox/meshes/sponza");
    auto pink_texture = renderer->upload_texture(FileSystem::load_texture("sandbox/textures/pink_checkerboard"));
    auto green_texture = renderer->upload_texture(FileSystem::load_texture("sandbox/textures/green_checkerboard"));
    auto marble_texture = renderer->upload_texture(FileSystem::load_texture("sandbox/textures/marble"));

    auto floor = world.create_entity();
    floor.add<Material>(marble_texture);
    floor.add<Mesh>(cube_mesh);
    floor.add<Transform>(glm::vec3(0.0f, 57.75f, 0.0f), glm::vec3(1000.0f, 1.0f, 1000.0f));

    Array<std::tuple<glm::vec3, glm::vec3>, 6> wall_positions{{
        {glm::vec3(80.0f, 77.75f, 30.0f), glm::vec3(100.0f, 20.0f, 1.0f)},
        {glm::vec3(80.0f, 77.75f, 150.0f), glm::vec3(100.0f, 20.0f, 1.0f)},
        {glm::vec3(180.0f, 77.75f, 90.0f), glm::vec3(1.0f, 20.0f, 60.0f)},
        {glm::vec3(50.0f, 67.75f, 129.0f), glm::vec3(1.0f, 10.0f, 20.0f)},
        {glm::vec3(60.0f, 67.75f, 110.0f), glm::vec3(10.0f, 10.0f, 1.0f)},
        {glm::vec3(59.5f, 78.75f, 129.0f), glm::vec3(10.5f, 1.0f, 20.0f)},
    }};
    for (const auto &[position, scale] : wall_positions) {
        auto wall = world.create_entity();
        wall.add<Material>(pink_texture);
        wall.add<Mesh>(cube_mesh);
        wall.add<Transform>(position, scale);
    }

    Array sponza_positions{
        glm::vec3(107.0f, 60.0f, 67.0f),
    };
    for (const auto &position : sponza_positions) {
        auto sponza = world.create_entity();
        sponza.add<Material>(green_texture);
        sponza.add<Mesh>(sponza_mesh);
        sponza.add<Transform>(position, glm::vec3(0.01f));
    }

    for (int i = 0; i < 300; i++) {
        auto &light = renderer->lights().emplace();
        light.colour = glm::linearRand(glm::vec3(0.1f), glm::vec3(0.4f));
        light.radius = glm::linearRand(15.0f, 30.0f);
        light.position.x = glm::linearRand(-20.0f, 180.0f);
        light.position.y = glm::linearRand(60.0f, 100.0f);
        light.position.z = glm::linearRand(35.0f, 145.0f);
    }

    Vector<glm::vec3> control_points;
    control_points.emplace(-161.02f, 115.69f, 70.64f);
    control_points.emplace(-107.69f, 92.76f, 69.63f);
    control_points.emplace(24.73f, 50.82f, 69.45f);
    control_points.emplace(72.78f, 65.68f, 68.59f);
    control_points.emplace(114.56f, 83.58f, 65.31f);
    control_points.emplace(125.37f, 81.32f, 53.99f);
    control_points.emplace(157.41f, 93.11f, 96.09f);
    control_points.emplace(183.38f, 100.05f, 138.31f);
    control_points.emplace(168.50f, 84.70f, 142.59f);
    control_points.emplace(82.43f, 70.32f, 132.39f);

    auto curve_points = BezierCurve::construct(control_points, 0.005f);
    auto follower = world.create_entity();
    follower.add<PointFollower>(curve_points, 15.0f);
    follower.add<Transform>(glm::vec3(curve_points[0]));

    auto &ubo = renderer->ubo();
    ubo.proj = glm::perspective(glm::radians(45.0f), window.aspect_ratio(), 0.1f, 1000.0f);
    ubo.proj[1][1] *= -1;

    Vector<float> frame_times;
    frame_times.ensure_capacity(100000);

    double previous_time = glfwGetTime();
    while (!window.should_close()) {
        double current_time = glfwGetTime();
        auto dt = static_cast<float>(current_time - previous_time);
        previous_time = current_time;

        frame_times.push(dt);
        if (frame_times.size() > 1000 && follower.get<PointFollower>()->next_point() == 0) {
            break;
        }

        auto &position = follower.get<Transform>()->position();
        auto &orientation = follower.get<Transform>()->orientation();
        ubo.camera_position = position;
        ubo.view =
            glm::lookAt(position, position + orientation * glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        world.update(dt);
        { ZoneScopedN("Window::poll_events") Window::poll_events(); }
        FrameMark
    }
    FileSystem::deinitialise();

    std::fstream csv("out.csv", std::ios::out | std::ios::trunc);
    csv << "frame,frame_time\n";
    for (std::uint32_t i = 0; i < frame_times.size(); i++) {
        csv << i << ',' << (frame_times[i] * 1000) << '\n';
    }
    csv.flush();
}
