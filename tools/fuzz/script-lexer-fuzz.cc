#include <vull/script/lexer.hh>
#include <vull/script/token.hh>
#include <vull/support/string.hh>
#include <vull/support/utility.hh>

#include <stddef.h>
#include <stdint.h>

using namespace vull;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    auto string = String::copy_raw(reinterpret_cast<const char *>(data), size);
    script::Lexer lexer("", string);
    while (lexer.peek().kind() != script::TokenKind::Eof) {
        auto token = lexer.next();
        VULL_DO_NOT_OPTIMIZE(lexer.recover_position(token));
    }
    return 0;
}
