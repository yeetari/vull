#include <vull/core/Window.hh>
#include <vull/vulkan/Context.hh>

int main() {
    vull::Window window(800, 600, false);
    vull::Context context;
    auto swapchain = window.create_swapchain(context);
    while (!window.should_close()) {
        window.poll_events();
    }
}
