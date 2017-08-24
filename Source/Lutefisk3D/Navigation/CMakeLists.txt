set(INCLUDES
    CrowdAgent.h
    CrowdManager.h
    DynamicNavigationMesh.h
    NavArea.h
    NavBuildData.h
    Navigable.h
    NavigationEvents.h

    Obstacle.h
    OffMeshConnection.h

)
set(SOURCE
    CrowdAgent.cpp
    CrowdManager.cpp
    DynamicNavigationMesh.cpp
    NavArea.cpp
    NavBuildData.cpp
    Navigable.cpp
    NavigationMesh.cpp
    Obstacle.cpp
    OffMeshConnection.cpp
)
add_library(Lutefisk3D_Navigation STATIC ${SOURCE} ${INCLUDES}
    $<TARGET_OBJECTS:Detour>
    $<TARGET_OBJECTS:DetourCrowd>
    $<TARGET_OBJECTS:Recast>
    $<TARGET_OBJECTS:DetourTileCache>
)

target_include_directories(Lutefisk3D_Navigation PRIVATE
    $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Recast,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Detour,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:DetourCrowd,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:DetourTileCache,INTERFACE_INCLUDE_DIRECTORIES>
)
target_compile_definitions(Lutefisk3D_Navigation PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>)
set_property(TARGET Lutefisk3D_Navigation PROPERTY POSITION_INDEPENDENT_CODE ON)

add_dependencies(Lutefisk3D_Navigation Detour DetourCrowd)
set_property(TARGET Lutefisk3D_Navigation PROPERTY INTERFACE_LINK_LIBRARIES
    Lutefisk3D_Physics
#    Detour
#    DetourCrowd
)

if(UNIT_TESTING)
#add_lutefisk_test(AttributeTests)
#add_lutefisk_test(ContextTests)
endif()
