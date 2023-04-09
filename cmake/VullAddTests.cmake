get_filename_component(pwd "${TEST_EXECUTABLE}" DIRECTORY)

execute_process(COMMAND "${TEST_EXECUTABLE}" --list-tests
        WORKING_DIR "${pwd}"
        OUTPUT_VARIABLE list_stdout
        ERROR_VARIABLE list_stderr
        RESULT_VARIABLE list_result)

if(NOT ${list_result} EQUAL 0)
    message(FATAL_ERROR "Failed to discover tests: ${list_stdout}${list_stderr}")
endif()

string(REPLACE "\n" ";" list "${list_stdout}")
set(script)
foreach(test ${list})
    string(APPEND script "add_test(\"${test}\" \"${TEST_EXECUTABLE}\" \"${test}\")\n")
endforeach()
file(WRITE vull_tests.cmake "${script}")
