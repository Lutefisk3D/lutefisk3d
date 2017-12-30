cmake_minimum_required(VERSION 3.7)
project(Lutefisk3D CXX C)
#SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
#set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "/usr/bin/time -f \"%E : %C -> Inputs %I, Outputs %O\"")
SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake;${CMAKE_MODULE_PATH})

include(cotire)
set(COTIRE_ADD_UNITY_BUILD FALSE)
ADD_DEFINITIONS(-D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS)
IF(MSVC)
    ADD_DEFINITIONS(-D_CRT_NONSTDC_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS -DWIN32 -DNOMINMAX)
#    ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS -D__UNIX__ )
#    ADD_DEFINITIONS(/W4)
ELSE()
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Wno-unused-parameter -Wno-sign-compare -Wno-invalid-offsetof")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -std=c99")
    if(MINGW)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-attributes")
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-attributes")
    endif()
    SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} "  ) #--coverage
ENDIF()
set (CMAKE_CXX_STANDARD 14)
#set(CMAKE_AUTOMOC ON)
SET(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 COMPONENTS Core Xml Gui REQUIRED)

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
