function(add_fuzz_test name)
    vull_add_executable(${name}-fuzz ${ARGN})
    target_link_options(${name}-fuzz PRIVATE -fsanitize=fuzzer)
endfunction()

add_fuzz_test(json json-fuzz.cc)
if(VULL_BUILD_SCRIPT)
    add_fuzz_test(script-lexer script-lexer-fuzz.cc)
endif()
