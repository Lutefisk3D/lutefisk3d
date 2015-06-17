cmake_minimum_required(VERSION 3.0.0)
project(Lutefisk3D)

SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake;${CMAKE_MODULE_PATH})

ADD_DEFINITIONS(-D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS)
IF(CMAKE_BUILD_TOOL MATCHES "(msdev|devenv|nmake)")
    ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS -D__UNIX__ -D_CRT_NONSTDC_NO_DEPRECATE)
    ADD_DEFINITIONS(/W4)
ELSE()
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Wno-unused-parameter -Wno-sign-compare -std=c++11")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -std=c99")
    SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} "  ) #--coverage
ENDIF()

find_package(Qt5Core 5.5.0)
find_package(Qt5Xml  5.5.0)


add_subdirectory(Source)


#add_subdirectory(Client)
#add_subdirectory(Common)
#add_subdirectory(Server)

