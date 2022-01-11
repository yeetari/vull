#include <vull/core/Window.hh>

int main() {
    vull::Window window(800, 600);
    while (!window.should_close()) {
        window.poll_events();
    }
}
