#pragma once

namespace vull::tasklet {

struct IoQueue;

} // namespace vull::tasklet

namespace vull::platform {

void spawn_tasklet_io_dispatcher(tasklet::IoQueue &queue);

} // namespace vull::platform
