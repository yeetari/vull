#pragma once

#include <vull/support/Optional.hh>
#include <vull/support/StringView.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/vpak/PackFile.hh> // IWYU pragma: keep

namespace vull {

class File;

} // namespace vull

namespace vull::vpak {

class ReadStream;

void load_vpak(File &&file);
UniquePtr<ReadStream> open(StringView name);
Optional<vpak::Entry> stat(StringView name);

} // namespace vull::vpak
