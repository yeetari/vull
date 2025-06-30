#pragma once

#include <vull/core/input.hh>

#include <xkbcommon/xkbcommon.h>

namespace vull::platform {
Key xkb_translate_key(xkb_keysym_t keysym);
} // namespace vull::platform
