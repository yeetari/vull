#pragma once

#include <vull/support/optional.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/vpak/pack_file.hh> // IWYU pragma: keep

namespace vull {

class File;

} // namespace vull

namespace vull::vpak {

class ReadStream;

void load_vpak(File &&file);
UniquePtr<ReadStream> open(StringView name);
Optional<Entry> stat(StringView name);

} // namespace vull::vpak
