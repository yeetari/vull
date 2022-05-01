# TODO: Being an object library might increase the build time for executables linking to vull, investigate when vull
#       is a bit bigger.
add_library(vull OBJECT)
add_subdirectory(sources)
target_add_shader(vull shaders/light_cull.comp)
target_add_shader(vull shaders/main.vert)
target_add_shader(vull shaders/main.frag)
target_add_shader(vull shaders/shadow.vert)
target_add_shader(vull shaders/terrain.vert)
target_add_shader(vull shaders/terrain.frag)
target_add_shader(vull shaders/ui.vert)
target_add_shader(vull shaders/ui.frag)
target_compile_features(vull PRIVATE cxx_std_20)
target_compile_options(vull PRIVATE -fno-exceptions -fno-rtti -nostdinc++)
target_include_directories(vull PUBLIC include PRIVATE sources)
# TODO: Use X11::xcb_randr target when available in cmake.
target_link_libraries(vull PRIVATE
    Freetype::Freetype
    harfbuzz::harfbuzz
    X11::xcb
    xcb-randr
    X11::xcb_util
    X11::xcb_xkb
    Zstd::Zstd)

if(VULL_BUILD_TESTS)
    add_executable(vull-tests)
    add_subdirectory(tests)
    target_compile_features(vull-tests PRIVATE cxx_std_20)
    target_link_libraries(vull-tests PRIVATE vull)
endif()

# TODO: Use cmake presets for this.
if(VULL_BUILD_WARNINGS)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(vull PUBLIC
            -Werror
            -Weverything
            -Wno-c++98-compat
            -Wno-c++98-compat-pedantic
            -Wno-covered-switch-default
            -Wno-double-promotion
            -Wno-error=deprecated-declarations
            # Disabling this should be fine as it's now allowed in C++ 20.
            -Wno-gnu-zero-variadic-macro-arguments
            -Wno-non-virtual-dtor
            -Wno-padded
            -Wno-shadow-uncaptured-local
            -Wno-switch
            -Wno-switch-enum
            -Wno-weak-vtables)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(vull PUBLIC
            -Werror
            -Wall
            -Wextra
            -Wno-class-memaccess
            -Wno-double-promotion
            -Wno-missing-field-initializers
            -Wno-non-virtual-dtor
            -Wno-switch
            -Wno-unknown-pragmas)
    endif()
endif()
