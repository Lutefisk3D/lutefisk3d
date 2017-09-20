include_directories(
    $<TARGET_PROPERTY:Recast,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Detour,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:DetourCrowd,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:DetourTileCache,INTERFACE_INCLUDE_DIRECTORIES>
)
set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/CrowdAgent.h
    ${CMAKE_CURRENT_SOURCE_DIR}/CrowdManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DynamicNavigationMesh.h
    ${CMAKE_CURRENT_SOURCE_DIR}/NavArea.h
    ${CMAKE_CURRENT_SOURCE_DIR}/NavBuildData.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Navigable.h
    ${CMAKE_CURRENT_SOURCE_DIR}/NavigationEvents.h

    ${CMAKE_CURRENT_SOURCE_DIR}/Obstacle.h
    ${CMAKE_CURRENT_SOURCE_DIR}/OffMeshConnection.h

)
set(SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/CrowdAgent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CrowdManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DynamicNavigationMesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/NavArea.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/NavBuildData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Navigable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/NavigationMesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Obstacle.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/OffMeshConnection.cpp
)

install(FILES ${INCLUDES} DESTINATION include/Lutefisk3D/Navigation )

if(UNIT_TESTING)
#add_lutefisk_test(AttributeTests)
#add_lutefisk_test(ContextTests)
endif()

set(Lutefisk3D_LINK_LIBRARIES ${Lutefisk3D_LINK_LIBRARIES} Detour DetourCrowd Recast DetourTileCache PARENT_SCOPE)
set(Lutefisk3D_COMPONENT_SOURCES ${Lutefisk3D_COMPONENT_SOURCES} ${SOURCE} ${INCLUDES} PARENT_SCOPE)
