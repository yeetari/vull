add_executable(vull-benchmark main.cc)
target_compile_features(vull-benchmark PRIVATE cxx_std_20)
target_enable_optimisations(vull-benchmark)
target_link_libraries(vull-benchmark PRIVATE vull-engine)
