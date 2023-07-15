# TODO: Create add_fuzz_test function or macro.

vull_executable(lexer-fuzz vull::core vull::script)
target_sources(lexer-fuzz PRIVATE lexer-fuzz.cc)
target_link_options(lexer-fuzz PRIVATE -fsanitize=fuzzer)

vull_executable(json-fuzz vull::core vull::json)
target_sources(json-fuzz PRIVATE json-fuzz.cc)
target_link_options(json-fuzz PRIVATE -fsanitize=fuzzer)
