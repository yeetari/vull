FetchContent_Declare(vull-sandbox-assets
    GIT_REPOSITORY https://github.com/yeetari/vull-sandbox-assets.git
    GIT_TAG 9607d956e178211130798e3a4aa87d3e04a73983
    GIT_PROGRESS ON
    SOURCE_DIR assets)
FetchContent_MakeAvailable(vull-sandbox-assets)

# TODO: Try to get this properly working where the engine defines this and any of the engine's dependants transitively
#       build/copy the builtin pack.
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/engine/shaders)
add_custom_target(vull-builtin-pack)
add_custom_command(
    TARGET vull-builtin-pack
    POST_BUILD
    COMMAND vull-pack write builtin.vpak ${CMAKE_BINARY_DIR}/engine/shaders
    VERBATIM)
target_add_shader(vull-builtin-pack ../engine/shaders/depth.vert)
target_add_shader(vull-builtin-pack ../engine/shaders/light_cull.comp)
target_add_shader(vull-builtin-pack ../engine/shaders/main.frag)
target_add_shader(vull-builtin-pack ../engine/shaders/main.vert)

add_custom_command(
    OUTPUT sandbox.vpak
    COMMAND vull-pack write sandbox.vpak assets
    VERBATIM)

add_executable(vull-sandbox Config.cc main.cc VehicleController.cc sandbox.vpak)
add_dependencies(vull-sandbox vull-builtin-pack)
target_compile_features(vull-sandbox PRIVATE cxx_std_20)
target_enable_optimisations(vull-sandbox)
target_link_libraries(vull-sandbox PRIVATE vull-engine)
