if(UNIX)
    set(OPENAL_LIBRARY_IMP libGLEW.a)
    set(OPENAL_LIBRARY_SHARED libopenal.so)
    set(openal_LIBRARY_IMP ${ThirdParty_Install_Dir}/lib/${OPENAL_LIBRARY_IMP})
    set(openal_LIBRARY_SHARED ${ThirdParty_Install_Dir}/lib/${OPENAL_LIBRARY_SHARED})
elseif(MSVC)
    set(OPENAL_LIBRARY_IMP OpenAL32.lib)
    set(OPENAL_LIBRARY_SHARED OpenAL32.dll)
    set(openal_LIBRARY_IMP ${ThirdParty_Install_Dir}/lib/${OPENAL_LIBRARY_IMP})
    set(openal_LIBRARY_SHARED ${ThirdParty_Install_Dir}/bin/${OPENAL_LIBRARY_SHARED})
elseif(MINGW)
    set(OPENAL_LIBRARY_IMP libOpenAL32.dll.a)
    set(OPENAL_LIBRARY_SHARED OpenAL32.dll)
    set(openal_LIBRARY_IMP ${ThirdParty_Install_Dir}/lib/${OPENAL_LIBRARY_IMP})
    set(openal_LIBRARY_SHARED ${ThirdParty_Install_Dir}/bin/${OPENAL_LIBRARY_SHARED})
endif()

ExternalProject_Add(
   openal_BUILD
   URL ${CMAKE_CURRENT_SOURCE_DIR}/openal-soft-1.19.0.tar.bz2
   UPDATE_COMMAND ""
   INSTALL_DIR ${ThirdParty_Install_Dir}
   CMAKE_ARGS -DALSOFT_EXAMPLES:BOOL=FALSE -DCMAKE_INSTALL_LIBDIR:PATH=lib -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE} -DCMAKE_STAGING_PREFIX:PATH=${ThirdParty_Install_Dir} -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
   CMAKE_GENERATOR "${CMAKE_GENERATOR}"
   CMAKE_GENERATOR_TOOLSET ${CMAKE_GENERATOR_TOOLSET}
   BUILD_BYPRODUCTS ${openal_LIBRARY_SHARED} ${openal_LIBRARY_STATIC} ${openal_LIBRARY_IMP}
)
ADD_LIBRARY(openal_IMP SHARED IMPORTED GLOBAL)
add_dependencies(openal_IMP openal_BUILD)
set_target_properties(openal_IMP PROPERTIES
    IMPORTED_IMPLIB ${openal_LIBRARY_IMP}
    IMPORTED_LOCATION ${openal_LIBRARY_SHARED}
)

