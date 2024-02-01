#include <vull/vpak/file_system.hh>

#include <vull/container/vector.hh>
#include <vull/support/optional.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/vpak/pack_file.hh>
#include <vull/vpak/reader.hh>

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

Optional<Entry> stat(StringView name) {
    // TODO: This is hashing name for every vpak.
    for (const auto &reader : s_loaded_vpaks) {
        if (auto entry = reader->stat(name)) {
            return *entry;
        }
    }
    return {};
}

} // namespace vull::vpak
