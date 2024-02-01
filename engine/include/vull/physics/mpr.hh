#pragma once

#include <vull/physics/contact.hh> // IWYU pragma: keep
#include <vull/support/optional.hh>

namespace vull {

struct Shape;
class Transform;

Optional<Contact> mpr_test(const Shape &s1, const Transform &t1, const Shape &s2, const Transform &t2);

} // namespace vull
