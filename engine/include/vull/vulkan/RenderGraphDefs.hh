#pragma once

#include <stdint.h>

namespace vull::vk {

// TODO: Make ResourceId opaque and non-copyable.
// TODO: Wrangle IWYU to have a forward declaration of RenderGraph here.
using ResourceId = uint32_t;

} // namespace vull::vk
