# TODO: Being an object library might increase the build time for executables linking to vull, investigate when vull
#       is a bit bigger.
add_library(vull OBJECT)
add_subdirectory(sources)
target_add_shader(vull shaders/light_cull.comp)
target_add_shader(vull shaders/terrain.vert)
target_add_shader(vull shaders/terrain.frag)
target_compile_definitions(vull PUBLIC VK_NO_PROTOTYPES)
target_compile_features(vull PRIVATE cxx_std_20)
target_compile_options(vull PRIVATE -fno-exceptions -fno-rtti -nostdinc++)
target_include_directories(vull PUBLIC include PRIVATE sources)
target_link_libraries(vull PRIVATE X11::xcb X11::xcb_util X11::xcb_xkb)

if(VULL_BUILD_WARNINGS)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(vull PUBLIC
            -Werror
            -Weverything
            -Wno-c++98-compat
            -Wno-c++98-compat-pedantic
            # Disabling this should be fine as it's now allowed in C++ 20.
            -Wno-gnu-zero-variadic-macro-arguments
            -Wno-padded)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(vull PUBLIC
            -Werror
            -Wall
            -Wextra
            -Wno-class-memaccess
            -Wno-missing-field-initializers
            -Wno-unknown-pragmas)
    endif()
endif()
