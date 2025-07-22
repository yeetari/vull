#pragma once

#include <stddef.h>
#include <stdint.h>

namespace vull::tasklet {

struct IoQueue;

} // namespace vull::tasklet

namespace vull::platform {

uint8_t *allocate_fiber_memory(size_t size);
void spawn_tasklet_io_dispatcher(tasklet::IoQueue &queue);

} // namespace vull::platform
