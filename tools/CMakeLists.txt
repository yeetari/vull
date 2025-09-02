if(CMAKE_CXX_COMPILER MATCHES "(afl-clang-lto)+")
    add_subdirectory(fuzz)
endif()

vull_add_executable(mpmc-bench mpmc_bench.cc)
vull_add_executable(tasklet-bench tasklet_bench.cc)
vull_add_executable(tlsf-bench tlsf_bench.cc)

if(VULL_BUILD_VPAK)
    FetchContent_Declare(meshoptimizer
        GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
        GIT_TAG v0.23
        GIT_SHALLOW TRUE
        SYSTEM)
    FetchContent_MakeAvailable(meshoptimizer)

    vull_add_executable(vpak
        vpak/enc/bc5enc.cc
        vpak/enc/bc7enc.cc
        vpak/float_image.cc
        vpak/gltf_parser.cc
        vpak/mad_inst.cc
        vpak/mad_lut.cc
        vpak/main.cc
        vpak/png_stream.cc)
    target_include_directories(vpak SYSTEM PRIVATE vpak/enc)
    target_link_libraries(vpak PRIVATE PNG::PNG Zstd::Zstd meshoptimizer)

    configure_file(vpak/mad_lut.bin.zst ${CMAKE_CURRENT_BINARY_DIR}/mad_lut.bin.zst COPYONLY)
    set_source_files_properties(vpak/enc/bc5enc.cc PROPERTIES COMPILE_FLAGS -w)
    set_source_files_properties(vpak/enc/bc7enc.cc PROPERTIES COMPILE_FLAGS -w)

    vull_add_executable(vpak-build-lut
        vpak/build-lut.cc
        vpak/mad_inst.cc)
endif()

if(VULL_BUILD_SCRIPT)
    vull_add_executable(vsi vsi.cc)
endif()

vull_add_executable(vslc
    vslc/spv/backend.cc
    vslc/spv/builder.cc
    vslc/ast.cc
    vslc/char_stream.cc
    vslc/legaliser.cc
    vslc/lexer.cc
    vslc/main.cc
    vslc/parser.cc
    vslc/token.cc)

vull_add_executable(vslc-new vslc.cc)
target_link_libraries(vslc-new PRIVATE vull::shaderc)
