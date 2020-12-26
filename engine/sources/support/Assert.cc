#include <vull/support/Assert.hh>

#include <cstdlib>
#include <iostream>

[[noreturn]] void assertion_failed(const char *file, unsigned int line, const char *expr) {
    std::cout << "Assertion '" << expr << "' failed at " << file << ':' << line << '\n';
    std::abort();
}
