add_executable(vull
    renderer/Camera.cc
    renderer/Device.cc
    renderer/Instance.cc
    renderer/Surface.cc
    renderer/Swapchain.cc
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
target_link_libraries(vull PRIVATE glfw glm::glm Vulkan::Vulkan)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-ffast-math" COMPILER_SUPPORTS_FAST_MATH)
    if(COMPILER_SUPPORTS_FAST_MATH)
        target_compile_options(vull PRIVATE "-ffast-math")
        target_link_options(vull PRIVATE "-ffast-math")
    endif()
    if(NOT DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION)
        set_property(TARGET vull PROPERTY INTERPROCEDURAL_OPTIMIZATION ON)
    endif()
endif()
