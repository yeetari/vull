#include <vull/json/parser.hh>
#include <vull/json/tree.hh> // IWYU pragma: keep
#include <vull/support/result.hh>
#include <vull/support/string.hh>

#include <stddef.h>
#include <stdint.h>

using namespace vull;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    auto string = String::copy_raw(reinterpret_cast<const char *>(data), size);
    auto parse_result = json::parse(string);
    return parse_result.is_error() ? -1 : 0;
}
