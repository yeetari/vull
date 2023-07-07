vull_executable(lexer-fuzz vull::core vull::script)
target_sources(lexer-fuzz PRIVATE lexer-fuzz.cc)
target_link_options(lexer-fuzz PRIVATE -fsanitize=fuzzer)
