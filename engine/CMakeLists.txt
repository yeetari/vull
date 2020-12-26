add_library(vull-engine)
add_subdirectory(shaders)
add_subdirectory(sources)
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
    ${CMAKE_SOURCE_DIR}/third-party
    PRIVATE
    sources)
target_link_libraries(vull-engine
    PUBLIC
    glfw
    glm::glm
    Vulkan::Vulkan)
