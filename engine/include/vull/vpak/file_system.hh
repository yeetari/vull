#pragma once

#include <vull/support/optional.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/vpak/defs.hh>

namespace vull::vpak {

class ReadStream;

void load_vpak(StringView name, String path);
UniquePtr<ReadStream> open(StringView name);
Optional<Entry> stat(StringView name);

} // namespace vull::vpak
