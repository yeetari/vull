vull_module(core Zstd::Zstd)
vull_module(graphics vull::core vull::vulkan)
vull_module(json vull::core)
vull_module(main vull::core)
vull_module(physics vull::core)
vull_module(script vull::core)
vull_module(ui vull::core vull::vulkan Freetype::Freetype harfbuzz::harfbuzz)
vull_module(vulkan vull::core)
vull_module(x11-window vull::core X11::xcb X11::xcb_randr X11::xcb_util X11::xcb_xkb)

add_subdirectory(sources)
target_include_directories(vull-core PUBLIC include)
target_include_directories(vull-script PRIVATE sources)

# Default shaders.
target_add_shader(vull-graphics shaders/blit_tonemap.frag)
target_add_shader(vull-graphics shaders/default.frag)
target_add_shader(vull-graphics shaders/default.vert)
target_add_shader(vull-graphics shaders/deferred.comp)
target_add_shader(vull-graphics shaders/depth_reduce.comp)
target_add_shader(vull-graphics shaders/draw_cull.comp)
target_add_shader(vull-graphics shaders/fst.vert)
target_add_shader(vull-graphics shaders/light_cull.comp)
target_add_shader(vull-graphics shaders/shadow.vert)
target_add_shader(vull-graphics shaders/skybox.frag)
target_add_shader(vull-graphics shaders/skybox.vert)
target_add_shader(vull-graphics shaders/ui.frag)
target_add_shader(vull-graphics shaders/ui.vert)

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
