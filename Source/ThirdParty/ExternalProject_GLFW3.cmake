FIND_PACKAGE(OpenGL REQUIRED)
add_subdirectory(glfw3)
set_target_properties(glfw PROPERTIES FOLDER ThirdParty)

if(false)
libname(glfw3 glfw3)

ExternalProject_Add(
    glfw3_BUILD
    DOWNLOAD_COMMAND ""
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/glfw3
    UPDATE_COMMAND ""
    INSTALL_DIR ${ThirdParty_Install_Dir}
    CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE} -DCMAKE_STAGING_PREFIX:PATH=${ThirdParty_Install_Dir} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DBUILD_SHARED_LIBS:BOOL=OFF
    CMAKE_GENERATOR "${CMAKE_GENERATOR}"
    CMAKE_GENERATOR_TOOLSET ${CMAKE_GENERATOR_TOOLSET}
    BUILD_BYPRODUCTS ${glfw3_LIBRARY_STATIC}
)
if(MSVC)
    ADD_LIBRARY(glfw3_IMP STATIC IMPORTED GLOBAL) # this is shared but cmake is wonky on windows and tries to link to glfw3.lib instead of the set glfw3dll.lib
elseif(MINGW)
    ADD_LIBRARY(glfw3_IMP STATIC IMPORTED GLOBAL) # this is shared but cmake is wonky on windows and tries to link to glfw3.lib instead of the set glfw3dll.lib
else()
    ADD_LIBRARY(glfw3_IMP STATIC IMPORTED GLOBAL)
    set(glfw3_LIBRARY_STATIC ${glfw3_LIBRARY_STATIC})
endif()
add_dependencies(glfw3_IMP glfw3_BUILD)
SET_TARGET_PROPERTIES(glfw3_IMP PROPERTIES
    IMPORTED_LINK_INTERFACE_LIBRARIES "${OPENGL_LIBRARIES}"
    IMPORTED_LOCATION ${glfw3_LIBRARY_STATIC}
    IMPORTED_LOCATION_DEBUG ${glfw3_LIBRARY_STATIC}
    IMPORTED_LOCATION_RELEASE ${glfw3_LIBRARY_STATIC}
)

if(WIN32)
    set_property(TARGET glfw3_IMP PROPERTY IMPORTED_IMPLIB ${glfw3_LIBRARY_STATIC})
endif()
endif()
