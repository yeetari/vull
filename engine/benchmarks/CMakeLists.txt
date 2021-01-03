add_executable(vull-benchmarks
    EntitySystemBenchmark.cc)
target_compile_features(vull-benchmarks PRIVATE cxx_std_20)
target_enable_optimisations(vull-benchmarks)
target_link_libraries(vull-benchmarks PRIVATE benchmark::benchmark_main vull-engine)
