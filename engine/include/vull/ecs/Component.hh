#pragma once

#include <stddef.h>

#define VULL_DECLARE_COMPONENT(name)                                                                                   \
public:                                                                                                                \
    static constexpr auto k_component_id = static_cast<size_t>(name)
