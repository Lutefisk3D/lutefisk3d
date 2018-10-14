cmake_minimum_required(VERSION 3.7)
project(Lutefisk3D CXX C)

#SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
#set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "/usr/bin/time -f \"%E : %C -> Inputs %I, Outputs %O\"")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake;${CMAKE_MODULE_PATH})

# Include file that sets up all configurable properties
include(LutefiskOptions)
# Include common utilitles
include(LutefiskCommon)

include(CMakeDependentOption)
include(CheckCSourceCompiles)
include(cotire)

set(COTIRE_ADD_UNITY_BUILD FALSE)
ADD_DEFINITIONS(-D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS)
IF(MSVC)
    ADD_DEFINITIONS(-D_CRT_NONSTDC_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS -DWIN32 -DNOMINMAX)
#    ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS -D__UNIX__ )
#    ADD_DEFINITIONS(/W4)
ELSE()
    SET(LF_CMAKE_CXX_FLAGS "-Wall -Wextra -pedantic -Wno-unused-parameter -Wno-sign-compare -Wno-invalid-offsetof")
    SET(LF_CMAKE_C_FLAGS "-Wall -Wextra -pedantic -std=c99")
    if(MINGW)
        SET(LF_CMAKE_CXX_FLAGS "-Wno-attributes")
        SET(LF_CMAKE_C_FLAGS "-Wno-attributes")
    endif()
    SET(LF_CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG} ) #--coverage
ENDIF()
if (NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif ()
#set(CMAKE_AUTOMOC ON)
SET(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 COMPONENTS Core Xml Gui Widgets Quick REQUIRED)
#Widgets only used to display message box in one place.. consider removal ?
#Quick only used if QUI is selected
find_package(Threads REQUIRED)

set(LUTEFISK3D_AS_DLLS TRUE)
# adds testing options and add_lutefisk_test macro
include(LutefiskTestingFacilities)

# ExternalProjects are used to compile/install projects contained inside ThirdParty directory
include(ExternalProject)
set(ThirdParty_Install_Dir ${CMAKE_CURRENT_BINARY_DIR}/3rd_party/built)
INCLUDE_DIRECTORIES(${ThirdParty_Install_Dir}/include)


#set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE /usr/bin/include-what-you-use)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(Source)

install(DIRECTORY bin/CoreData DESTINATION share/Lutefisk3D/)

# Print current build configuration
message(STATUS "LUTEFISK3D Configuration:")
if (LUTEFISK3D_STATIC_RUNTIME)
    message(STATUS "  Runtime         STATIC")
else ()
    message(STATUS "  Runtime         SHARED")
endif ()
message(STATUS "  Library         ${LUTEFISK3D_LIBRARY_TYPE}")
message(STATUS "  SSE             ${LUTEFISK3D_SSE}")
message(STATUS "  2D              ${LUTEFISK3D_2D}")
message(STATUS "  AUDIO           ${LUTEFISK3D_SOUND}")
message(STATUS "  IK              ${LUTEFISK3D_IK}")
message(STATUS "  Threading       ${LUTEFISK3D_THREADING}")
message(STATUS "  Navigation      ${LUTEFISK3D_NAVIGATION}")
message(STATUS "  Network         ${LUTEFISK3D_NETWORK}")
message(STATUS "  Physics         ${LUTEFISK3D_PHYSICS}")
message(STATUS "  Samples         ${LUTEFISK3D_SAMPLES}")
message(STATUS "  WebP            ${LUTEFISK3D_WEBP}")
message(STATUS "  CSharp          ${LUTEFISK3D_CSHARP}")
message(STATUS "Developer options:")
message(STATUS "  Packaging       ${LUTEFISK3D_PACKAGING}")
message(STATUS "  SystemUI        ${LUTEFISK3D_SYSTEMUI}")
message(STATUS "  Logging         ${LUTEFISK3D_LOGGING}")
message(STATUS "  Profiling       ${LUTEFISK3D_PROFILING}")
message(STATUS "  Extras          ${LUTEFISK3D_EXTRAS}")
message(STATUS "  Tools           ${LUTEFISK3D_TOOLS}")
if (TARGET Profiler)
    message(STATUS "     Profiler GUI ${LUTEFISK3D_PROFILING}")
endif ()

# clang-tidy target
find_program(CLANG_TIDY clang-tidy)
if (CLANG_TIDY)
    file (GLOB_RECURSE SOURCE_FILES
        Source/Samples/**.cpp Source/Samples/**.h Source/Samples/**.hpp Source/Samples/**.inl
        Source/Lutefisk3D/**.cpp Source/Lutefisk3D/**.h Source/Lutefisk3D/**.hpp
        Source/Tools/**.cpp Source/Tools/**.h Source/Tools/**.hpp
    )
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY} -p . -export-fixes=clang-tidy.yml -fix ${SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_target_properties(tidy PROPERTIES EXCLUDE_FROM_ALL 1 EXCLUDE_FROM_DEFAULT_BUILD 1)
endif()
