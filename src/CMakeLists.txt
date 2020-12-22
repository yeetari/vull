add_executable(vull
    renderer/Camera.cc
    support/Assert.cc
    main.cc
    Window.cc)
target_add_shader(vull shaders/depth.vert)
target_add_shader(vull shaders/light_cull.comp)
target_add_shader(vull shaders/main.vert)
target_add_shader(vull shaders/main.frag)
target_compile_definitions(vull PRIVATE GLFW_INCLUDE_VULKAN GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_RADIANS)
target_compile_features(vull PRIVATE cxx_std_20)
target_include_directories(vull PRIVATE . ../third-party)
target_link_libraries(vull PRIVATE assimp fmt::fmt glfw glm::glm Vulkan::Vulkan)
