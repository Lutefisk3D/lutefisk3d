set(INCLUDES
    CrowdAgent.h
    DetourCrowdManager.h
    DynamicNavigationMesh.h
    NavArea.h
    NavBuildData.h
    Navigable.h

    Obstacle.h
    OffMeshConnection.h

)
set(SOURCE
    CrowdAgent.cpp
    DetourCrowdManager.cpp
    DynamicNavigationMesh.cpp
    NavArea.cpp
    NavBuildData.cpp
    Navigable.cpp
    NavigationMesh.cpp
    Obstacle.cpp
    OffMeshConnection.cpp
)
add_library(Lutefisk3D_Navigation OBJECT ${SOURCE} ${INCLUDES})
add_dependencies(Lutefisk3D_Navigation LibCpuId)
target_include_directories(Lutefisk3D_Navigation PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(Lutefisk3D_Navigation PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>)
set_property(TARGET Lutefisk3D_Navigation PROPERTY POSITION_INDEPENDENT_CODE ON)

add_dependencies(Lutefisk3D_Navigation SDL2_LIB)

######################### Add component to the project's list
list(APPEND Lutefisk3D_components $<TARGET_OBJECTS:Lutefisk3D_Navigation>)
SET(Lutefisk3D_components ${Lutefisk3D_components} PARENT_SCOPE)

if(UNIT_TESTING)
#add_lutefisk_test(AttributeTests)
#add_lutefisk_test(ContextTests)
endif()
