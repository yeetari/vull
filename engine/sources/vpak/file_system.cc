#include <vull/vpak/file_system.hh>

#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/support/optional.hh>
#include <vull/support/result.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/vpak/defs.hh>
#include <vull/vpak/pack_file.hh>
#include <vull/vpak/stream.hh>

namespace vull::vpak {
namespace {

VULL_GLOBAL(Vector<UniquePtr<PackFile>> s_loaded_vpaks);

} // namespace

void load_vpak(StringView name, String path) {
    auto pack_file_or_error = PackFile::open(vull::move(path));
    if (!pack_file_or_error.is_error()) {
        auto pack_file = vull::adopt_unique(pack_file_or_error.disown_value());
        vull::info("[vpak] Loaded vpak '{}' ({} entries)", name, pack_file->entries().size());
        s_loaded_vpaks.push(vull::move(pack_file));
        return;
    }
    // TODO: Print error details.
    vull::error("[vpak] Failed to load vpak '{}'", name);
}

UniquePtr<ReadStream> open(StringView name) {
    // TODO: This is hashing name for every vpak.
    for (const auto &pack_file : s_loaded_vpaks) {
        if (auto stream = pack_file->open_entry(name)) {
            return vull::move(stream);
        }
    }
    return {};
}

Optional<Entry> stat(StringView name) {
    // TODO: This is hashing name for every vpak.
    for (const auto &pack_file : s_loaded_vpaks) {
        if (auto entry = pack_file->stat(name)) {
            return *entry;
        }
    }
    return {};
}

} // namespace vull::vpak
