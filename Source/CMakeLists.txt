INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR}) # for export header
INCLUDE_DIRECTORIES(ThirdParty)
INCLUDE_DIRECTORIES(${PROJECT_BINARY_DIR}/ThirdParty_Builds/include)
INCLUDE_DIRECTORIES(${PROJECT_BINARY_DIR}/ThirdParty_Builds/include/bullet)
LINK_DIRECTORIES(${PROJECT_BINARY_DIR}/ThirdParty_Builds/lib)

# list of OBJECT libraries will be collected here
set(ThirdParty_requisites)

add_subdirectory(ThirdParty) # add required third party libraries to the build

if(LUTEFISK3D_SSE AND MINGW)
    set(LUTEFISK3D_SSE 0)
    message("Disabled sse since gcc 5.3 does not align the stacks")
endif()
if(LUTEFISK3D_SSE)
    IF(MSVC)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /arch:SSE2")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:SSE2")
    ELSE()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse2")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse2")
    ENDIF()
endif()
if (NOT CMAKE_CROSS_COMPILING AND LUTEFISK3D_TOOLS)
    add_subdirectory (Tools)
    if (LUTEFISK3D_EXTRAS)
        add_subdirectory (Extras)
    endif ()
endif ()

add_subdirectory(Lutefisk3D)
target_include_directories(Lutefisk3D INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>  
    $<INSTALL_INTERFACE:include> 
)
if(LUTEFISK3D_SAMPLES)
    add_subdirectory(Samples)
endif()
