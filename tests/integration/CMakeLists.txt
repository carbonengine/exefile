# Copyright © 2026 CCP ehf.

find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_package(carbon-blue CONFIG REQUIRED)

# CPython keeps its extension modules outside the standard library, in a folder whose name differs by platform (DLLs on
# Windows, lib-dynload elsewhere) and that FindPython does not report. Ask the interpreter where one of them lives.
execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import _ctypes, os; print(os.path.dirname(_ctypes.__file__))"
    OUTPUT_VARIABLE python_extensions
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# ExeFile loads Blue by file name, so Blue goes beside the executable, as it does when shipped.
add_custom_target(ExefileTestBlue ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Blue> $<TARGET_FILE_DIR:Exefile>
    VERBATIM
)
add_dependencies(ExefileTestBlue Exefile)

execute_process(
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/discover.py
    OUTPUT_VARIABLE discovered
    ERROR_VARIABLE discovery_errors
    RESULT_VARIABLE discovery_result
)
if(discovery_result)
    message(FATAL_ERROR "Discovering the integration tests failed:\n${discovery_errors}")
endif()
# Python on Windows ends lines with \r\n.
string(REGEX MATCHALL "[^\r\n]+" tests "${discovered}")
# Without this, a case added to run_tests.py stays out of CTest until someone reconfigures by hand.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS discover.py run_tests.py)

foreach(test IN LISTS tests)
    add_test(
        NAME ${test}
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_tests.py ${test}
    )
    set_tests_properties(${test} PROPERTIES
        ENVIRONMENT_MODIFICATION
            "EXEFILE=set:$<TARGET_FILE:Exefile>;EXEFILE_LIB=set:${Python3_STDLIB};EXEFILE_LIB=path_list_append:${python_extensions}"
    )
endforeach()
