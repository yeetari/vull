separate_arguments(VULL_COMPILER_FLAGS_LIST NATIVE_COMMAND "${VULL_COMPILER_FLAGS}")

function(vull_add_executable name)
    add_executable(${name} ${ARGN})
    target_compile_features(${name} PRIVATE cxx_std_20)
    target_compile_options(${name} PRIVATE ${VULL_COMPILER_FLAGS_LIST})
    target_link_libraries(${name} PRIVATE vull::vull)
endfunction()
