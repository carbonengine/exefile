# Copyright © 2021 CCP ehf.
cmake_minimum_required(VERSION 3.16)

# vcpkg installs the manifest's features when project() runs, so this has to come first.
option(BUILD_TESTING "Build the integration tests, which builds Blue through vcpkg" OFF)
if(BUILD_TESTING)
    list(APPEND VCPKG_MANIFEST_FEATURES "tests")
endif()

project(Exefile VERSION 1.0.0)

include(cmake/CcpTargetConfigurations.cmake)
include(cmake/CcpDocsGenerator.cmake)
include(cmake/CcpPackageConfigHelpers.cmake)
include(cmake/CcpBuildConfigurations.cmake)

find_package(crashpad CONFIG REQUIRED)
find_package(carbon-core CONFIG REQUIRED)
find_package(Python3 COMPONENTS Development REQUIRED)

ccp_add_executable(Exefile
        autoversion.h
        BlueInterface.cpp
        BlueInterface.h
        CommandArguments.cpp
        CommandArguments.h
        Crashpad.cpp
        Crashpad.h
        ExeFile.cpp
        ExeFile.h
        ExeFileOsX.mm
        ExeFileWin32.cpp
        FileSystem.h
        FileSystemOsX.mm
        FileSystemWin32.cpp
        resource.h
        StdAfx.cpp
        StdAfx.h
        version.h
        # Icon resource file on Windows, ignored on macOS
        ExeFile.rc
        )

if(WIN32)
    # manifest file contains supported OS versions for windows
    target_sources(Exefile PRIVATE .manifest)
endif()

target_precompile_headers(Exefile PRIVATE StdAfx.h)

set_target_properties(Exefile PROPERTIES OUTPUT_NAME exefile)

if(WIN32)
    target_link_libraries(Exefile PRIVATE Winmm)
    set_target_properties(Exefile PROPERTIES WIN32_EXECUTABLE True)
endif()

if(APPLE)
    enable_language(OBJCXX)
    target_link_libraries(Exefile PRIVATE "-framework Foundation")
    set_target_properties(
            Exefile
            PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "@executable_path/."
            # turn off IPO/LTO for macOS because it has issues with mixing opaque and non-opaque pointers
            INTERPROCEDURAL_OPTIMIZATION OFF
    )
endif()

target_compile_definitions(Exefile PRIVATE BLUEBUILD_STATIC CRASH_REPORTS_ENABLED)

target_link_libraries(Exefile PUBLIC CcpCore crashpad::crashpad Python3::Python)

target_include_directories(Exefile
        PRIVATE
        ${BLUE_INCLUDE_DIRECTORY}
        )

set_target_properties(Exefile PROPERTIES OUTPUT_NAME "exefile")
set_target_properties(Exefile PROPERTIES CMAKE_ARCHIVE_OUTPUT_DIRECTORY  "exefile")

# Provide tests and an install target iff this is the top level project only
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    if(BUILD_TESTING)
        enable_testing()
        add_subdirectory(tests/integration)
    endif()

    set (INSTALL_TO_MONOLITH OFF CACHE BOOL "Install this package to a perforce branch vendor folder - provided for backwards compatability")

    if (INSTALL_TO_MONOLITH)
        # Install rule to ensure that our runtime and linker files are in the expected, platform-specific folders
        install(
                IMPORTED_RUNTIME_ARTIFACTS crashpad::handler
                DESTINATION bin/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
        )
        install(
                TARGETS Exefile
                EXPORT ExefileTargets
                CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES}
                RUNTIME DESTINATION bin/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
        )
    else() # "VCPKG" INSTALL_MODE, but there's no need for another option, as this will be the default
        install(IMPORTED_RUNTIME_ARTIFACTS crashpad::handler)
        install(
                TARGETS Exefile
                EXPORT ExefileTargets
                CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES}
                RUNTIME DESTINATION bin/
        )
    endif()
endif()
