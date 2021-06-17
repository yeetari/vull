add_executable(vull-pack main.cc)
target_compile_features(vull-pack PRIVATE cxx_std_20)
target_enable_optimisations(vull-pack)
target_link_libraries(vull-pack PRIVATE vull-engine)
