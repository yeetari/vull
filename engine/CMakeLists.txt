add_library(vull-engine)
if(VULL_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
add_subdirectory(sources)
if(VULL_BUILD_TESTS)
    add_subdirectory(tests)
endif()
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
    PRIVATE
    sources)
target_include_directories(vull-engine SYSTEM PUBLIC ${CMAKE_SOURCE_DIR}/third-party)
target_link_libraries(vull-engine
    PUBLIC
    glfw
    glm::glm
    Threads::Threads
    Vulkan::Vulkan)
