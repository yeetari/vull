function(target_add_vpak target name)
    get_target_property(vpak_path ${target} BINARY_DIR)
    cmake_path(APPEND vpak_path ${vpak_path} ${name})
    foreach(i RANGE 2 ${ARGC} 2)
        math(EXPR j "${i} + 1")
        list(APPEND item_list "${ARGV${i}}" "${ARGV${j}}")
        list(APPEND dependency_list "${ARGV${i}}")
    endforeach()

    add_custom_command(OUTPUT ${name}
        COMMAND rm -f ${vpak_path}
        COMMAND vpak add ${vpak_path} ${item_list}
        DEPENDS "${dependency_list}")
    set_property(TARGET ${target} APPEND PROPERTY SOURCES ${name})
endfunction()
