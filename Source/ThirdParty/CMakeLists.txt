add_subdirectory(cereal)
add_subdirectory(LibCpuId)
add_subdirectory(PugiXml)
add_subdirectory(LZ4)

add_subdirectory(rapidjson)

if(NOT LUTEFISK3D_HEADLESS)
    ExternalProject_Add(
        SDL2
        DOWNLOAD_COMMAND ""
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SDL
        UPDATE_COMMAND ""
        CMAKE_ARGS -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSDL_SHARED:BOOL=OFF -DVIDEO_OPENGLES:BOOL=OFF -DSDL_FILESYSTEM:BOOL=OFF -DSDL_RENDERER:BOOL=OFF -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
        INSTALL_DIR ${PROJECT_SOURCE_DIR}/Source/ThirdParty_Builds/
    )
    ADD_LIBRARY(SDL2_LIB STATIC IMPORTED GLOBAL)
    add_dependencies(SDL2_LIB SDL2)
    if(UNIX)
        SET_TARGET_PROPERTIES(SDL2_LIB PROPERTIES IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/Source/ThirdParty_Builds/lib/libSDL2.a)
    else()
        message(ERROR "TODO")
        SET_TARGET_PROPERTIES(SDL2_LIB PROPERTIES IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/Source/ThirdParty_Builds/lib/SDL2.lib)
    endif()

    ExternalProject_Add(
        glbinding
        DOWNLOAD_COMMAND ""
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/glbinding
        UPDATE_COMMAND ""
        CMAKE_ARGS -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPTION_BUILD_STATIC:BOOL=ON -DOPTION_BUILD_TESTS:BOOL=OFF -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR> -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER} -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
        INSTALL_DIR ${PROJECT_SOURCE_DIR}/Source/ThirdParty_Builds/
    )
    ADD_LIBRARY(glbinding_LIB STATIC IMPORTED GLOBAL)
    add_dependencies(glbinding_LIB glbinding)
    FIND_PACKAGE(OpenGL REQUIRED)
    if(UNIX)
        SET_TARGET_PROPERTIES(glbinding_LIB PROPERTIES
            IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/Source/ThirdParty_Builds/lib/libglbinding.a
            IMPORTED_LINK_INTERFACE_LIBRARIES "${OPENGL_LIBRARIES}"
        )

    else()
        message(ERROR "TODO")
        SET_TARGET_PROPERTIES(glbinding_LIB PROPERTIES IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/Source/ThirdParty_Builds/lib/glbinding.lib)
    endif()

endif()
if(NOT LUTEFISK3D_NAVIGATIONLESS)
    add_subdirectory(Recast)
    add_subdirectory(Detour)
    add_subdirectory(DetourTileCache)
    add_subdirectory(DetourCrowd)
endif()
if(NOT LUTEFISK3D_SOUNDLESS)
    add_subdirectory(STB)
endif()


if(NOT LUTEFISK3D_NETLESS)
    ExternalProject_Add(
        kNet
        DOWNLOAD_COMMAND ""
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/kNet
        UPDATE_COMMAND ""
        CMAKE_ARGS -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
        INSTALL_DIR ${PROJECT_SOURCE_DIR}/Source/ThirdParty_Builds/
    )
    ADD_LIBRARY(kNet_LIB STATIC IMPORTED GLOBAL)
    add_dependencies(kNet_LIB kNet)
    if(UNIX)
        SET_TARGET_PROPERTIES(kNet_LIB PROPERTIES IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/Source/ThirdParty_Builds/lib/libkNet.a)
    else()
        message(ERROR "TODO")
        SET_TARGET_PROPERTIES(kNet_LIB PROPERTIES IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/Source/ThirdParty_Builds/lib/kNet.lib)
    endif()

endif()
if(NOT LUTEFISK3D_UILESS)
    add_subdirectory(FreeType)
endif()
if(NOT LUTEFISK3D_2DLESS)
    add_subdirectory(Box2D)
endif()

SET(ThirdParty_requisites ${ThirdParty_requisites} PARENT_SCOPE)
