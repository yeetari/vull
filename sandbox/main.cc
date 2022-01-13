#include <vull/core/Window.hh>
#include <vull/vulkan/Context.hh>

int main() {
    vull::Window window(800, 600);
    vull::Context context;
    while (!window.should_close()) {
        window.poll_events();
    }
}
