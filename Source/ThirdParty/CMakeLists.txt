include(ExternalProject)

function(encode_lib_name basename)
    if(UNIX)
        set(${basename}_LIBRARY_SHARED lib${basename}.so PARENT_SCOPE)
        set(${basename}_LIBRARY_SHARED_D lib${basename}.so PARENT_SCOPE)
        set(${basename}_LIBRARY_STATIC lib${basename}.a PARENT_SCOPE)
        set(${basename}_LIBRARY_STATIC_D lib${basename}.a PARENT_SCOPE)
        set(${basename}_LIBRARY_IMP ${basename}.so PARENT_SCOPE)
        set(${basename}_LIBRARY_IMP_D ${basename}.so PARENT_SCOPE)
    elseif(MSVC)
        set(${basename}_LIBRARY_SHARED ${basename}.dll PARENT_SCOPE)
        set(${basename}_LIBRARY_SHARED_D ${basename}d.dll PARENT_SCOPE)
        set(${basename}_LIBRARY_STATIC ${basename}.lib PARENT_SCOPE)
        set(${basename}_LIBRARY_STATIC_D ${basename}d.lib PARENT_SCOPE)
        set(${basename}_LIBRARY_IMP ${basename}.lib PARENT_SCOPE)
        set(${basename}_LIBRARY_IMP_D ${basename}d.lib PARENT_SCOPE)
    elseif(MINGW)
        set(${basename}_LIBRARY_STATIC lib${basename}.a PARENT_SCOPE)
        set(${basename}_LIBRARY_STATIC_D lib${basename}.a PARENT_SCOPE)
        set(${basename}_LIBRARY_SHARED lib${basename}.dll PARENT_SCOPE)
        set(${basename}_LIBRARY_SHARED_D lib${basename}.dll PARENT_SCOPE)
        set(${basename}_LIBRARY_IMP lib${basename}.dll PARENT_SCOPE)
        set(${basename}_LIBRARY_IMP_D lib${basename}.dll PARENT_SCOPE)
    endif()
endfunction()
function(libname tgt basename)
    encode_lib_name(${basename})
    set(${tgt}_LIBRARY_STATIC ${ThirdParty_Install_Dir}/lib/${${basename}_LIBRARY_STATIC} PARENT_SCOPE)
    set(${tgt}_LIBRARY_STATIC_D ${ThirdParty_Install_Dir}/lib/${${basename}_LIBRARY_STATIC_D} PARENT_SCOPE)
    set(${tgt}_LIBRARY_SHARED ${ThirdParty_Install_Dir}/lib/${${basename}_LIBRARY_SHARED} PARENT_SCOPE)
    set(${tgt}_LIBRARY_SHARED_D ${ThirdParty_Install_Dir}/lib/${${basename}_LIBRARY_SHARED_D} PARENT_SCOPE)
    set(${tgt}_LIBRARY_IMP ${ThirdParty_Install_Dir}/lib/${${basename}_LIBRARY_IMP} PARENT_SCOPE)
    set(${tgt}_LIBRARY_IMP_D ${ThirdParty_Install_Dir}/lib/${${basename}_LIBRARY_IMP_D} PARENT_SCOPE)
endfunction()

macro(set_shared_lib_properties basename)
    SET_PROPERTY(TARGET ${basename}_IMP APPEND PROPERTY IMPORTED_LOCATION ${${basename}_LIBRARY_SHARED} )
    SET_PROPERTY(TARGET ${basename}_IMP APPEND PROPERTY IMPORTED_LOCATION_DEBUG ${${basename}_LIBRARY_SHARED_D} )
    SET_PROPERTY(TARGET ${basename}_IMP APPEND PROPERTY IMPORTED_LOCATION_RELEASE ${${basename}_LIBRARY_SHARED} )
    SET_PROPERTY(TARGET ${basename}_IMP APPEND PROPERTY IMPORTED_IMPLIB ${${basename}_LIBRARY_IMP} )
    SET_PROPERTY(TARGET ${basename}_IMP APPEND PROPERTY IMPORTED_IMPLIB_RELEASE ${${basename}_LIBRARY_IMP} )
    SET_PROPERTY(TARGET ${basename}_IMP APPEND PROPERTY IMPORTED_IMPLIB_DEBUG ${${basename}_LIBRARY_IMP_D} )
endmacro()
macro(set_static_lib_properties basename)
    SET_PROPERTY(TARGET ${basename}_IMP APPEND PROPERTY IMPORTED_LOCATION ${${basename}_LIBRARY_STATIC} )
    SET_PROPERTY(TARGET ${basename}_IMP APPEND PROPERTY IMPORTED_LOCATION_DEBUG ${${basename}_LIBRARY_STATIC_D} )
    SET_PROPERTY(TARGET ${basename}_IMP APPEND PROPERTY IMPORTED_LOCATION_RELEASE ${${basename}_LIBRARY_STATIC} )
endmacro()
link_directories(${ThirdParty_Install_Dir}/lib)

include(ExternalProject_CEREAL.cmake)
add_subdirectory(jlsignal)
#add_subdirectory(LibCpuId)
add_subdirectory(PugiXml)
add_subdirectory(LZ4)

add_subdirectory(rapidjson)

# *_BuiltFile is setting up the location of the built library - needed for ninja generator
if(NOT UNIX)
        message(ERROR "TODO: Verify the lib paths for non UNIX builds")
endif()
set(ThirdParty_Install_Dir ${PROJECT_BINARY_DIR}/ThirdParty_Builds)
if(NOT LUTEFISK3D_HEADLESS)
    if(UNIX OR MINGW)
        set(SDL2_BuiltFile ${ThirdParty_Install_Dir}/lib/libSDL2.a)
    else()
        set(SDL2_BuiltFile ${ThirdParty_Install_Dir}/lib/SDL2.lib)
    endif()

    ExternalProject_Add(
        SDL2
        DOWNLOAD_COMMAND ""
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SDL
        UPDATE_COMMAND ""
        CMAKE_ARGS -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSDL_SHARED:BOOL=OFF -DSDL_STATIC_PIC:BOOL=ON -DRENDER_D3D:BOOL=OFF -DVIDEO_OPENGLES:BOOL=OFF -DSDL_FILESYSTEM:BOOL=ON -DVIDEO_WAYLAND:BOOL=OFF -DVIDEO_MIR:BOOL=OFF -DSDL_RENDERER:BOOL=OFF -DCMAKE_INSTALL_PREFIX:PATH=${ThirdParty_Install_Dir} -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER} -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
        INSTALL_DIR ${ThirdParty_Install_Dir}/
        BUILD_BYPRODUCTS ${SDL2_BuiltFile}
    )
    ADD_LIBRARY(SDL2_LIB STATIC IMPORTED GLOBAL)
    SET_TARGET_PROPERTIES(SDL2_LIB PROPERTIES IMPORTED_LOCATION ${SDL2_BuiltFile})
    add_dependencies(SDL2_LIB SDL2)

    if(UNIX OR MINGW)
        set(GLBINDING_BuiltFile ${ThirdParty_Install_Dir}/lib/libglbinding.a)
        set(GLBINDING_BuiltFileD ${ThirdParty_Install_Dir}/lib/libglbinding.a)
    else()
        set(GLBINDING_BuiltFile ${ThirdParty_Install_Dir}/lib/glbinding.lib)
        set(GLBINDING_BuiltFileD ${ThirdParty_Install_Dir}/lib/glbindingd.lib)
    endif()
    ExternalProject_Add(
        glbinding
        DOWNLOAD_COMMAND ""
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/glbinding
        UPDATE_COMMAND ""
        CMAKE_ARGS -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_SHARED_LIBS:BOOL=OFF -DOPTION_BUILD_TESTS:BOOL=OFF -DCMAKE_INSTALL_PREFIX:PATH=${ThirdParty_Install_Dir} -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER} -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
        INSTALL_DIR ${ThirdParty_Install_Dir}/
        BUILD_BYPRODUCTS ${GLBINDING_BuiltFile}
    )
    ADD_LIBRARY(glbinding_LIB STATIC IMPORTED GLOBAL)
    add_dependencies(glbinding_LIB glbinding)
    FIND_PACKAGE(OpenGL REQUIRED)
    SET_TARGET_PROPERTIES(glbinding_LIB PROPERTIES
        IMPORTED_LOCATION ${GLBINDING_BuiltFile}
        IMPORTED_LOCATION_DEBUG ${GLBINDING_BuiltFileD}
        IMPORTED_LINK_INTERFACE_LIBRARIES "${OPENGL_LIBRARIES}"
    )
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
    if(UNIX OR MINGW)
        set(KNET_BuiltFile ${ThirdParty_Install_Dir}/lib/libkNet.a)
    else()
        set(KNET_BuiltFile ${ThirdParty_Install_Dir}/lib/kNet.lib)
    endif()
    ExternalProject_Add(
        kNet
        DOWNLOAD_COMMAND ""
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/kNet
        UPDATE_COMMAND ""
        CMAKE_ARGS -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX:PATH=${ThirdParty_Install_Dir} -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER} -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
        INSTALL_DIR ${ThirdParty_Install_Dir}/
        BUILD_BYPRODUCTS ${KNET_BuiltFile}
    )
    ADD_LIBRARY(kNet_LIB STATIC IMPORTED GLOBAL)
    add_dependencies(kNet_LIB kNet)
    SET_TARGET_PROPERTIES(kNet_LIB PROPERTIES IMPORTED_LOCATION ${KNET_BuiltFile})

endif()
if(NOT LUTEFISK3D_UILESS)
    add_subdirectory(FreeType)
endif()
if(NOT LUTEFISK3D_2DLESS)
    add_subdirectory(Box2D)
endif()

if(NOT LUTEFISK3D_PHYSICLESS)
	set(USE_MSVC_RUNTIME_LIBRARY_DLL TRUE)
    add_subdirectory(Bullet)
    add_subdirectory(StanHull)
endif()
