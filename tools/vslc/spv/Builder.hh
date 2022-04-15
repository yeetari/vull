#pragma once

#include "Spirv.hh"

#include <vull/support/Function.hh>
#include <vull/support/Span.hh>

#include <stdio.h>

namespace spv {

class Builder {
    Id m_next_id{0};

public:
    void write(vull::Function<void(Word)> write_word) const;
};

} // namespace spv
