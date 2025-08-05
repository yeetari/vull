# Main vull library.
add_library(vull)
target_compile_features(vull PRIVATE cxx_std_23)
target_compile_options(vull PRIVATE ${VULL_COMPILER_FLAGS_LIST})
target_include_directories(vull PUBLIC include)
target_link_libraries(vull PUBLIC xxHash::xxHash PRIVATE Zstd::Zstd liburing::liburing)

# Add preprocessor definitions for enabled components.
foreach(component ${VULL_ENABLED_COMPONENTS})
    target_compile_definitions(vull PUBLIC VULL_BUILD_${component})
endforeach()

if(VULL_BUILD_UI)
    target_link_libraries(vull PRIVATE Freetype::Freetype harfbuzz::harfbuzz)
endif()

if(VULL_BUILD_WAYLAND_WINDOW)
    add_library(vull-wl-protocols OBJECT)
    wayland_client_protocol_add(vull-wl-protocols
        PROTOCOL "${Wayland_protocols_dir}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml"
        BASENAME xdg-decoration)
    wayland_client_protocol_add(vull-wl-protocols
        PROTOCOL "${Wayland_protocols_dir}/stable/xdg-shell/xdg-shell.xml"
        BASENAME xdg-shell)
    wayland_client_protocol_add(vull-wl-protocols
        PROTOCOL "${Wayland_protocols_dir}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml"
        BASENAME pointer-constraints)
    wayland_client_protocol_add(vull-wl-protocols
        PROTOCOL "${Wayland_protocols_dir}/unstable/relative-pointer/relative-pointer-unstable-v1.xml"
        BASENAME relative-pointer)

    # VULL_BUILD_WAYLAND_WINDOW preprocessor definition added automatically.
    target_link_libraries(vull PRIVATE Wayland::client Wayland::cursor X11::xkbcommon vull-wl-protocols)
endif()

if(VULL_BUILD_X11_WINDOW)
    target_link_libraries(vull PRIVATE X11::xcb X11::xcb_randr X11::xcb_xinput X11::xkbcommon X11::xkbcommon_X11)
endif()

if(VULL_ENABLE_TRACY)
    target_link_libraries(vull PRIVATE Tracy::TracyClient)
endif()

# Separate shaderc library. This is a separate library rather than a component since you'd likely want the shader
# compiler to be built for development, but not shipped with the final game.
add_library(vull-shaderc)
target_compile_features(vull-shaderc PRIVATE cxx_std_23)
target_compile_options(vull-shaderc PRIVATE ${VULL_COMPILER_FLAGS_LIST})
target_link_libraries(vull-shaderc PRIVATE vull)

# Add alias targets.
add_library(vull::vull ALIAS vull)
add_library(vull::shaderc ALIAS vull-shaderc)

vull_add_shader(shaders/blit_tonemap.frag)
vull_add_shader(shaders/default.frag)
vull_add_shader(shaders/default.vert)
vull_add_shader(shaders/deferred.comp)
vull_add_shader(shaders/depth_reduce.comp)
vull_add_shader(shaders/draw_cull.comp)
vull_add_shader(shaders/fst.vert)
vull_add_shader(shaders/light_cull.comp)
vull_add_shader(shaders/object.vsl)
vull_add_shader(shaders/shadow.vert)
vull_add_shader(shaders/skybox.frag)
vull_add_shader(shaders/skybox.vert)
vull_add_shader(shaders/ui.frag)
vull_add_shader(shaders/ui.vert)

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

add_subdirectory(sources)
set_target_properties(vull PROPERTIES
    UNITY_BUILD ON
    UNITY_BUILD_MODE GROUP)
set_source_files_properties(
    sources/vulkan/buffer.cc
    sources/vulkan/command_buffer.cc
    sources/vulkan/descriptor_builder.cc
    sources/vulkan/fence.cc
    sources/vulkan/image.cc
    sources/vulkan/pipeline.cc
    sources/vulkan/query_pool.cc
    sources/vulkan/queue.cc
    sources/vulkan/semaphore.cc
    sources/vulkan/shader.cc
    sources/vulkan/swapchain.cc
    PROPERTIES UNITY_GROUP "vulkan-wrappers"
)

if(VULL_BUILD_TESTS)
    vull_add_executable(vull-tests)
    target_compile_options(vull-tests PRIVATE -Wno-ctad-maybe-unsupported;-Wno-float-equal;-Wno-shorten-64-to-32;-Wno-sign-conversion)
    target_link_libraries(vull-tests PRIVATE vull-shaderc)
    add_subdirectory(tests)

    add_custom_command(TARGET vull-tests
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
        -D "\"TEST_EXECUTABLE:FILEPATH=$<TARGET_FILE:vull-tests>\""
        -P \"${CMAKE_SOURCE_DIR}/cmake/VullAddTests.cmake\"
        BYPRODUCTS vull_tests.cmake)
    set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES vull_tests.cmake)
endif()
