
################# Engine Core ########################

add_subdirectory(Container)
add_subdirectory(Core)
add_subdirectory(Math)

add_subdirectory(IO)
add_subdirectory(Resource)

add_subdirectory(Scene)


add_library(Lutefisk3D STATIC
    $<TARGET_OBJECTS:Lutefisk3D_Containers>
    $<TARGET_OBJECTS:Lutefisk3D_Core>
    $<TARGET_OBJECTS:Lutefisk3D_IO>
    $<TARGET_OBJECTS:Lutefisk3D_Math>
    $<TARGET_OBJECTS:Lutefisk3D_Resource>
    $<TARGET_OBJECTS:Lutefisk3D_Scene>
    $<TARGET_OBJECTS:PugiXml>
    $<TARGET_OBJECTS:LibCpuId>
    $<TARGET_OBJECTS:LZ4>

)
target_link_libraries(Lutefisk3D Threads::Threads)
qt5_use_modules(Lutefisk3D Core Gui)

