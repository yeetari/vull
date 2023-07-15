#include <vull/json/Parser.hh>
#include <vull/json/Tree.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Result.hh>
#include <vull/support/String.hh>

#include <stddef.h>
#include <stdint.h>

using namespace vull;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    auto string = String::copy_raw(reinterpret_cast<const char *>(data), size);
    auto parse_result = json::parse(string);
    return parse_result.is_error() ? -1 : 0;
}
