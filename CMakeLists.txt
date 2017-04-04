cmake_minimum_required(VERSION 3.0.0)
project(Lutefisk3D)
#SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake;${CMAKE_MODULE_PATH})

ADD_DEFINITIONS(-D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS)
IF(MSVC)
    ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS -D__UNIX__ -D_CRT_NONSTDC_NO_DEPRECATE)
    ADD_DEFINITIONS(/W4)
ELSE()
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Wno-unused-parameter -Wno-sign-compare -Wno-invalid-offsetof -std=c++11")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -std=c99")
    SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} "  ) #--coverage
ENDIF()

#set(CMAKE_AUTOMOC ON)
SET(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5Core 5.5.0)
find_package(Qt5Xml  5.5.0)
find_package(Qt5Gui  5.5.0)
find_package(Threads REQUIRED)

# adds testing options and add_lutefisk_test macro
include(LutefiskTestingFacilities)

# ExternalProjects are used to compile/install projects contained inside ThirdParty directory
include(ExternalProject)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(Source)

