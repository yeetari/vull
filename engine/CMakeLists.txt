add_library(vull-engine)
if(VULL_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
add_subdirectory(shaders)
add_subdirectory(sources)
if(VULL_BUILD_TESTS)
    add_subdirectory(tests)
endif()
configure_file(include/vull/Config.hh.in include/vull/Config.hh)
target_compile_definitions(vull-engine
    PUBLIC
    GLFW_INCLUDE_VULKAN
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_RADIANS)
target_compile_features(vull-engine PRIVATE cxx_std_20)
target_enable_optimisations(vull-engine)
target_include_directories(vull-engine
    PUBLIC
    include
    ${CMAKE_CURRENT_BINARY_DIR}/include
    PRIVATE
    sources)
target_include_directories(vull-engine SYSTEM PUBLIC ${CMAKE_SOURCE_DIR}/third-party)
target_link_libraries(vull-engine
    PUBLIC
    glfw
    glm::glm
    Threads::Threads
    Vulkan::Vulkan)
