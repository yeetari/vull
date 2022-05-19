if(VULL_BUILD_VPAK_TOOL)
    include(FetchContent)
    FetchContent_Declare(meshoptimizer
        GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
        GIT_TAG v0.17
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(meshoptimizer)
    get_target_property(meshoptimizer_iid meshoptimizer INTERFACE_INCLUDE_DIRECTORIES)
    set_target_properties(meshoptimizer PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${meshoptimizer_iid}")

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

    add_executable(vpak vpak/main.cc)
    target_compile_features(vpak PRIVATE cxx_std_20)
    target_compile_options(vpak PRIVATE -fno-exceptions -fno-rtti)
    target_link_libraries(vpak PRIVATE meshoptimizer simdjson vull)
endif()

add_executable(vslc
    vslc/spv/Backend.cc
    vslc/spv/Builder.cc
    vslc/Ast.cc
    vslc/CharStream.cc
    vslc/Legaliser.cc
    vslc/Lexer.cc
    vslc/main.cc
    vslc/Parser.cc
    vslc/Token.cc)
target_compile_features(vslc PRIVATE cxx_std_20)
target_compile_options(vslc PRIVATE -fno-exceptions -fno-rtti -nostdinc++)
target_link_libraries(vslc PRIVATE vull)
