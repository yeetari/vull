vull_module(core xxHash::xxHash Zstd::Zstd)
vull_module(graphics vull::core vull::vulkan)
vull_module(json vull::core)
vull_module(main vull::core)
vull_module(physics vull::core)
vull_module(script vull::core)
vull_module(ui vull::core vull::vulkan PRIVATE Freetype::Freetype harfbuzz::harfbuzz)
vull_module(vulkan vull::core)
vull_module(x11-window vull::core PRIVATE X11::xcb X11::xcb_randr X11::xcb_util X11::xcb_xkb)

add_subdirectory(sources)
target_include_directories(vull-core PUBLIC include)
target_include_directories(vull-script PRIVATE sources)

add_shader(shaders/blit_tonemap.frag)
add_shader(shaders/default.frag)
add_shader(shaders/default.vert)
add_shader(shaders/deferred.comp)
add_shader(shaders/depth_reduce.comp)
add_shader(shaders/draw_cull.comp)
add_shader(shaders/fst.vert)
add_shader(shaders/light_cull.comp)
add_shader(shaders/object.vsl)
add_shader(shaders/shadow.vert)
add_shader(shaders/skybox.frag)
add_shader(shaders/skybox.vert)
add_shader(shaders/ui.frag)
add_shader(shaders/ui.vert)

function(vull_depend_builtin target)
    target_add_vpak(${target} builtin.vpak
        ${CMAKE_SOURCE_DIR}/engine/fonts/Inter-Medium.otf /fonts/Inter-Medium
        ${CMAKE_SOURCE_DIR}/engine/fonts/RobotoMono-Regular.ttf /fonts/RobotoMono-Regular
        ${CMAKE_BINARY_DIR}/engine/shaders/blit_tonemap.frag.spv /shaders/blit_tonemap.frag
        ${CMAKE_BINARY_DIR}/engine/shaders/default.frag.spv /shaders/default.frag
        ${CMAKE_BINARY_DIR}/engine/shaders/default.vert.spv /shaders/default.vert
        ${CMAKE_BINARY_DIR}/engine/shaders/deferred.comp.spv /shaders/deferred.comp
        ${CMAKE_BINARY_DIR}/engine/shaders/depth_reduce.comp.spv /shaders/depth_reduce.comp
        ${CMAKE_BINARY_DIR}/engine/shaders/draw_cull.comp.spv /shaders/draw_cull.comp
        ${CMAKE_BINARY_DIR}/engine/shaders/fst.vert.spv /shaders/fst.vert
        ${CMAKE_BINARY_DIR}/engine/shaders/light_cull.comp.spv /shaders/light_cull.comp
        ${CMAKE_BINARY_DIR}/engine/shaders/object.vsl.spv /shaders/object
        ${CMAKE_BINARY_DIR}/engine/shaders/shadow.vert.spv /shaders/shadow.vert
        ${CMAKE_BINARY_DIR}/engine/shaders/skybox.frag.spv /shaders/skybox.frag
        ${CMAKE_BINARY_DIR}/engine/shaders/skybox.vert.spv /shaders/skybox.vert
        ${CMAKE_BINARY_DIR}/engine/shaders/ui.frag.spv /shaders/ui.frag
        ${CMAKE_BINARY_DIR}/engine/shaders/ui.vert.spv /shaders/ui.vert)
endfunction()

set_target_properties(vull-vulkan PROPERTIES
    UNITY_BUILD ON
    UNITY_BUILD_MODE GROUP)
set_source_files_properties(
    sources/vulkan/Buffer.cc
    sources/vulkan/CommandBuffer.cc
    sources/vulkan/DescriptorBuilder.cc
    sources/vulkan/Fence.cc
    sources/vulkan/Image.cc
    sources/vulkan/Pipeline.cc
    sources/vulkan/QueryPool.cc
    sources/vulkan/Queue.cc
    sources/vulkan/Semaphore.cc
    sources/vulkan/Shader.cc
    sources/vulkan/Swapchain.cc
    PROPERTIES UNITY_GROUP "vulkan-wrappers"
)

if(VULL_BUILD_TESTS)
    add_executable(vull-tests)
    add_subdirectory(tests)
    target_compile_features(vull-tests PRIVATE cxx_std_20)
    target_link_libraries(vull-tests PRIVATE vull::core vull::json)

    add_custom_command(TARGET vull-tests
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
        -D "\"TEST_EXECUTABLE:FILEPATH=$<TARGET_FILE:vull-tests>\""
        -P \"${CMAKE_SOURCE_DIR}/cmake/VullAddTests.cmake\"
        BYPRODUCTS vull_tests.cmake)
    set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES vull_tests.cmake)
endif()
