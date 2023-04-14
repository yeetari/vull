#include <vull/vpak/FileSystem.hh>

#include <vull/container/Vector.hh>
#include <vull/support/Optional.hh>
#include <vull/support/StringView.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/vpak/PackFile.hh>
#include <vull/vpak/Reader.hh>

namespace vull::vpak {
namespace {

VULL_GLOBAL(Vector<UniquePtr<Reader>> s_loaded_vpaks);

} // namespace

void load_vpak(File &&file) {
    s_loaded_vpaks.push(vull::make_unique<Reader>(vull::move(file)));
}

UniquePtr<ReadStream> open(StringView name) {
    // TODO: This is hashing name for every vpak.
    for (const auto &reader : s_loaded_vpaks) {
        if (auto stream = reader->open(name)) {
            return vull::move(stream);
        }
    }
    return {};
}

Optional<vpak::Entry> stat(StringView name) {
    // TODO: This is hashing name for every vpak.
    for (const auto &reader : s_loaded_vpaks) {
        if (auto entry = reader->stat(name)) {
            return *entry;
        }
    }
    return {};
}

} // namespace vull::vpak
