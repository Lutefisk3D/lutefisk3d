include(GenerateExportHeader)

################# Engine Core ########################

macro(lutefisk_add_to_core Name)
    add_subdirectory(${Name})
    set_property(TARGET Lutefisk3D APPEND PROPERTY SOURCES $<TARGET_OBJECTS:Lutefisk3D_${Name}>)
    target_compile_definitions(Lutefisk3D_${Name} PRIVATE Lutefisk3D_EXPORTS)
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
add_library(Lutefisk3D SHARED
    ${MinimalSources}
)

lutefisk_add_to_core(Container)
lutefisk_add_to_core(Core)
lutefisk_add_to_core(Math)
lutefisk_add_to_core(IO)
lutefisk_add_to_core(Resource)
lutefisk_add_to_core(Scene)
lutefisk_add_to_core(Engine)

if(NOT LUTEFISK3D_HEADLESS)
    lutefisk_add_to_core(Graphics)
    if(NOT LUTEFISK3D_UILESS)
        lutefisk_add_to_core(UI)
    endif()
endif()

if(NOT LUTEFISK3D_INPUTLESS)
    lutefisk_add_to_core(Input)
endif()

if(NOT LUTEFISK3D_SOUNDLESS)
    lutefisk_add_to_core(Audio)
endif()
if(NOT LUTEFISK3D_NAVIGATIONLESS)
    lutefisk_add_to_core(Navigation)
endif()
if(NOT LUTEFISK3D_PHYSICLESS)
    lutefisk_add_to_core(Physics)
endif()
if(NOT LUTEFISK3D_2DLESS)
    lutefisk_add_to_core(2D)
endif()
GENERATE_EXPORT_HEADER(Lutefisk3D)

target_link_libraries(Lutefisk3D Threads::Threads ${CMAKE_DL_LIBS} ${ThirdParty_requisites})
#set_property(TARGET Lutefisk3D PROPERTY CXX_INCLUDE_WHAT_YOU_USE /usr/bin/include-what-you-use)

if(NOT LUTEFISK3D_HEADLESS)
    IF(WIN32)
         target_link_libraries(Lutefisk3D winmm imm32 version)
    endif()
endif()
if(NOT LUTEFISK3D_NETLESS)
    target_link_libraries(Lutefisk3D kNet_LIB) # add imported kNet target
endif()

qt5_use_modules(Lutefisk3D Core Gui)

