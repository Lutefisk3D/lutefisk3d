
################# Engine Core ########################

set(Lutefisk3D_components "")

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

if(NOT LUTEFISK3D_SOUNDLESS)
    add_subdirectory(Audio)
endif()

if(NOT LUTEFISK3D_INPUTLESS)
    add_subdirectory(Input)
endif()

if(NOT LUTEFISK3D_NAVIGATIONLESS)
    add_subdirectory(Navigation)
endif()
if(NOT LUTEFISK3D_NETLESS)
    add_subdirectory(Network)
endif()
if(NOT LUTEFISK3D_2DLESS)
    add_subdirectory(Urho2D)
endif()
if(NOT LUTEFISK3D_PHYSICLESS)
    add_subdirectory(Physics)
endif()


add_library(Lutefisk3D STATIC
    $<TARGET_OBJECTS:Lutefisk3D_Containers>
    $<TARGET_OBJECTS:Lutefisk3D_Core>
    $<TARGET_OBJECTS:Lutefisk3D_IO>
    $<TARGET_OBJECTS:Lutefisk3D_Math>
    $<TARGET_OBJECTS:Lutefisk3D_Resource>
    $<TARGET_OBJECTS:Lutefisk3D_Engine>
    $<TARGET_OBJECTS:Lutefisk3D_Scene>
    ${Lutefisk3D_components}
    ${ThirdParty_requisites}
)

target_link_libraries(Lutefisk3D Threads::Threads)
if(NOT LUTEFISK3D_HEADLESS)
    target_link_libraries(Lutefisk3D SDL2_LIB ${CMAKE_DL_LIBS}) # add imported SDL target
    target_link_libraries(Lutefisk3D glbinding_LIB) # add imported glbinding target
    IF(MINGW)
         target_link_libraries(Lutefisk3D winmm imm32 version)	
    endif()
endif()
if(NOT LUTEFISK3D_NETLESS)
    target_link_libraries(Lutefisk3D kNet_LIB) # add imported kNet target
endif()

if(NOT LUTEFISK3D_PHYSICLESS)
    target_link_libraries(Lutefisk3D Bullet_LIB) # add imported Bullet target
endif()

qt5_use_modules(Lutefisk3D Core Gui)

