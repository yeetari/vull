# TODO: Create add_fuzz_test function or macro.

vull_executable(script-lexer-fuzz vull::core vull::script)
target_sources(script-lexer-fuzz PRIVATE script-lexer-fuzz.cc)
target_link_options(script-lexer-fuzz PRIVATE -fsanitize=fuzzer)

vull_executable(json-fuzz vull::core vull::json)
target_sources(json-fuzz PRIVATE json-fuzz.cc)
target_link_options(json-fuzz PRIVATE -fsanitize=fuzzer)
