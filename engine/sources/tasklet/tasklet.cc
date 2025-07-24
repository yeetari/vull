#include <vull/tasklet/tasklet.hh>

#include <vull/tasklet/fiber.hh>

namespace vull::tasklet {

Tasklet *Tasklet::current() {
    return Fiber::current()->current_tasklet();
}

} // namespace vull::tasklet
