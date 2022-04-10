add_executable(vull-sandbox main.cc SceneLoader.cc)
target_compile_features(vull-sandbox PRIVATE cxx_std_20)
target_compile_options(vull-sandbox PRIVATE -fno-exceptions -fno-rtti -nostdinc++)
target_link_libraries(vull-sandbox PRIVATE vull)
