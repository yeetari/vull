find_package(assimp REQUIRED)

include(FetchContent)
FetchContent_Declare(meshoptimizer
    GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
    GIT_TAG v0.17
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(meshoptimizer)
get_target_property(meshoptimizer_iid meshoptimizer INTERFACE_INCLUDE_DIRECTORIES)
set_target_properties(meshoptimizer PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${meshoptimizer_iid}")

add_executable(vpak vpak/main.cc vpak/TextureLoader.cc)
target_compile_features(vpak PRIVATE cxx_std_20)
target_link_libraries(vpak PRIVATE assimp::assimp meshoptimizer vull)

add_executable(vslc
    vslc/spv/Backend.cc
    vslc/spv/Builder.cc
    vslc/Ast.cc
    vslc/CharStream.cc
    vslc/Lexer.cc
    vslc/main.cc
    vslc/Parser.cc
    vslc/Token.cc)
target_compile_features(vslc PRIVATE cxx_std_20)
target_compile_options(vslc PRIVATE -fno-exceptions -fno-rtti -nostdinc++)
target_link_libraries(vslc PRIVATE vull)
