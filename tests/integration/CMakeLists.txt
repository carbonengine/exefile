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

# Blue is built under a different name for each configuration (blue_debug, blue_internal, ...), and ExeFile picks the
# name from /buildflavor. vcpkg keeps Blue in the folder that also holds ExeFile's shared libraries. It copies those
# beside ExeFile only on Windows, and not for Debug, since every Carbon triplet installs release-type packages.
set(environment
    "EXEFILE=set:$<TARGET_FILE:Exefile>"
    "EXEFILE_BUILD_FLAVOR=set:$<LOWER_CASE:$<CONFIG>>"
    "EXEFILE_LIBRARY_DIR=set:$<TARGET_FILE_DIR:Blue>"
    "EXEFILE_LIB=set:${Python3_STDLIB}"
    "EXEFILE_LIB=path_list_append:${python_extensions}"
)

foreach(test IN LISTS tests)
    add_test(
        NAME ${test}
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_tests.py ${test}
    )
    set_tests_properties(${test} PROPERTIES ENVIRONMENT_MODIFICATION "${environment}")
endforeach()
