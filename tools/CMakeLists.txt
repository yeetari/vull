if(CMAKE_CXX_COMPILER MATCHES "(afl-clang-lto)+")
    add_subdirectory(fuzz)
endif()

vull_executable(vpak vull::core vull::graphics vull::json)
target_sources(vpak PRIVATE
    vpak/enc/bc7enc.cc
    vpak/enc/rgbcx.cc
    vpak/FloatImage.cc
    vpak/MadInst.cc
    vpak/MadLut.cc
    vpak/main.cc
    vpak/PngStream.cc)
target_include_directories(vpak SYSTEM PRIVATE vpak/enc)
set_source_files_properties(vpak/enc/bc7enc.cc PROPERTIES COMPILE_FLAGS -w)
set_source_files_properties(vpak/enc/rgbcx.cc PROPERTIES COMPILE_FLAGS -w)
configure_file(vpak/mad_lut.bin.zst ${CMAKE_CURRENT_BINARY_DIR}/mad_lut.bin.zst COPYONLY)

vull_executable(vpak-build-lut vull::core)
target_sources(vpak-build-lut PRIVATE vpak/build-lut.cc vpak/MadInst.cc)

if(VULL_BUILD_GLTF)
    include(FetchContent)
    FetchContent_Declare(meshoptimizer
        GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
        GIT_TAG v0.17
        GIT_SHALLOW TRUE
        SYSTEM)
    FetchContent_MakeAvailable(meshoptimizer)

    target_compile_definitions(vpak PRIVATE BUILD_GLTF)
    target_link_libraries(vpak PRIVATE meshoptimizer)
    target_sources(vpak PRIVATE vpak/GltfParser.cc)
endif()

if(VULL_BUILD_PNG)
    find_package(PNG REQUIRED)
    target_compile_definitions(vpak PRIVATE BUILD_PNG)
    target_link_libraries(vpak PRIVATE PNG::PNG)
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
