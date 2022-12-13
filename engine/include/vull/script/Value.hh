#pragma once

namespace vull::script {

using Number = double;

struct Object {};

union Value {
    Number number;
    Object *object;
};

} // namespace vull::script
