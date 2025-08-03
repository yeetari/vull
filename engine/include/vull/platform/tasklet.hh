#pragma once

#include <vull/support/function.hh>

#include <stddef.h>
#include <stdint.h>

namespace vull::tasklet {

template <typename>
class Future;
struct IoQueue;

} // namespace vull::tasklet

namespace vull::platform {

uint8_t *allocate_fiber_memory(size_t size);
void spawn_tasklet_io_dispatcher(tasklet::IoQueue &queue);
void take_over_main_thread(tasklet::Future<void> &&future, Function<void()> stop_fn);

} // namespace vull::platform
