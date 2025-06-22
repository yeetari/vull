find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    FetchContent_Declare(doxygen-awesome-css
        GIT_REPOSITORY https://github.com/jothepro/doxygen-awesome-css.git
        GIT_TAG v2.3.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(doxygen-awesome-css)
    FetchContent_GetProperties(doxygen-awesome-css SOURCE_DIR AWESOME_CSS_DIR)

    configure_file(Doxyfile.in Doxyfile @ONLY)
    add_custom_target(vull-docs
        COMMAND Doxygen::doxygen ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating documentation"
        VERBATIM)
endif()
