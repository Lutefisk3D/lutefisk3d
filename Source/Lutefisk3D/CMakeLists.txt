
################# Engine Core ########################

macro(add_lutefisk_component Name)
    add_subdirectory(${Name})
    set_property(TARGET Lutefisk3D APPEND PROPERTY INTERFACE_LINK_LIBRARIES Lutefisk3D_${Name} )
endmacro()

set(Lutefisk3D_components "")

set(MinimalSources
    Core/CoreEvents.cpp
    Core/CoreEvents.h
    Input/InputEvents.cpp
    Input/InputEvents.h
    Scene/SceneEvents.cpp
    Scene/SceneEvents.h
    Graphics/GraphicsEvents.cpp
    Graphics/GraphicsEvents.h
)
add_subdirectory(Container)
add_subdirectory(Core)
add_subdirectory(Math)
add_subdirectory(IO)
add_subdirectory(Resource)
add_subdirectory(Scene)
add_subdirectory(Engine)

if(NOT LUTEFISK3D_HEADLESS)
    add_subdirectory(Graphics)
    if(NOT LUTEFISK3D_UILESS)
        add_subdirectory(UI)
    endif()
endif()

if(NOT LUTEFISK3D_INPUTLESS)
    add_subdirectory(Input)
endif()

#if(NOT LUTEFISK3D_NETLESS)
#    add_subdirectory(Network)
#endif()


add_library(Lutefisk3D STATIC
    ${MinimalSources}
    ${Lutefisk3D_components}
    ${ThirdParty_requisites}
)

set_property(TARGET Lutefisk3D APPEND PROPERTY INTERFACE_LINK_LIBRARIES
    Lutefisk3D_Core
    Lutefisk3D_Containers
    Lutefisk3D_Engine
    Lutefisk3D_IO
    Lutefisk3D_Resource
    Lutefisk3D_Math
    Lutefisk3D_Scene
    Lutefisk3D_UI
    Lutefisk3D_Graphics
    )
if(NOT LUTEFISK3D_SOUNDLESS)
    add_lutefisk_component(Audio)
endif()
if(NOT LUTEFISK3D_NAVIGATIONLESS)
    add_lutefisk_component(Navigation)
endif()
if(NOT LUTEFISK3D_PHYSICLESS)
    add_lutefisk_component(Physics)
endif()
if(NOT LUTEFISK3D_2DLESS)
    add_lutefisk_component(2D)
endif()

target_link_libraries(Lutefisk3D Threads::Threads Lutefisk3D_Core Lutefisk3D_Containers)
#set_property(TARGET Lutefisk3D PROPERTY CXX_INCLUDE_WHAT_YOU_USE /usr/bin/include-what-you-use)

target_link_libraries(Lutefisk3D ${CMAKE_DL_LIBS}) # add imported SDL target
if(NOT LUTEFISK3D_HEADLESS)
    IF(WIN32)
         target_link_libraries(Lutefisk3D winmm imm32 version)
    endif()
endif()
if(NOT LUTEFISK3D_NETLESS)
    target_link_libraries(Lutefisk3D kNet_LIB) # add imported kNet target
endif()

qt5_use_modules(Lutefisk3D Core Gui)

