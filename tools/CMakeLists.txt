vull_executable(vpak vull::core vull::graphics)
target_sources(vpak PRIVATE vpak/main.cc)

# TODO: Split options, PNG support should be available regardless via another option.
if(VULL_BUILD_GLTF)
    include(FetchContent)
    FetchContent_Declare(meshoptimizer
        GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
        GIT_TAG v0.17
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(meshoptimizer)
    get_target_property(meshoptimizer_iid meshoptimizer INTERFACE_INCLUDE_DIRECTORIES)
    set_target_properties(meshoptimizer PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${meshoptimizer_iid}")

    find_package(PNG REQUIRED)

    set(SIMDJSON_DISABLE_DEPRECATED_API TRUE CACHE BOOL "" FORCE)
    set(SIMDJSON_ENABLE_THREADS FALSE CACHE BOOL "" FORCE)
    set(SIMDJSON_EXCEPTIONS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(simdjson
        GIT_REPOSITORY https://github.com/simdjson/simdjson.git
        GIT_TAG tags/v1.0.2
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(simdjson)
    get_target_property(simdjson_iid simdjson INTERFACE_INCLUDE_DIRECTORIES)
    set_target_properties(simdjson PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${simdjson_iid}")

    target_compile_definitions(vpak PRIVATE BUILD_GLTF)
    target_include_directories(vpak SYSTEM PRIVATE vpak/stb)
    target_link_libraries(vpak PRIVATE meshoptimizer PNG::PNG simdjson)
    target_sources(vpak PRIVATE vpak/GltfParser.cc vpak/PngStream.cc)
endif()

vull_executable(vsi vull::core vull::script)
target_sources(vsi PRIVATE vsi/main.cc)

vull_executable(vslc vull::core)
target_sources(vslc PRIVATE
    vslc/spv/Backend.cc
    vslc/spv/Builder.cc
    vslc/Ast.cc
    vslc/CharStream.cc
    vslc/Legaliser.cc
    vslc/Lexer.cc
    vslc/main.cc
    vslc/Parser.cc
    vslc/Token.cc)
