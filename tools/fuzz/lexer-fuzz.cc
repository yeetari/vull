#include <vull/script/Lexer.hh>
#include <vull/script/Token.hh>
#include <vull/support/Assert.hh>
#include <vull/support/String.hh>
#include <vull/support/Utility.hh>

#include <stddef.h>
#include <stdint.h>

using namespace vull;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    auto string = String::copy_raw(reinterpret_cast<const char *>(data), size);
    if (string.empty()) {
        return -1;
    }
    script::Lexer lexer("", string);
    while (lexer.peek().kind() != script::TokenKind::Eof) {
        auto token = lexer.next();
        VULL_DO_NOT_OPTIMIZE(lexer.recover_position(token));
    }
    return 0;
}
