set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/Animatable.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationDefs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AttributeAnimationInfo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Component.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LogicComponent.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Node.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ObjectAnimation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ReplicationState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Scene.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SceneEvents.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SceneResolver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Serializable.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SmoothedTransform.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SplinePath.h
    ${CMAKE_CURRENT_SOURCE_DIR}/UnknownComponent.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ValueAnimation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ValueAnimationInfo.h
)
set(SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/Animatable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AttributeAnimationInfo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Component.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Node.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/LogicComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ObjectAnimation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Scene.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SceneResolver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Serializable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SmoothedTransform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SplinePath.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/UnknownComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ValueAnimation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ValueAnimationInfo.cpp
)


install(FILES ${INCLUDES} DESTINATION include/Lutefisk3D/Scene )

if(UNIT_TESTING)
add_lutefisk_test(ValueAnimationTests)
endif()

#set(Lutefisk3D_LINK_LIBRARIES ${Lutefisk3D_LINK_LIBRARIES} PugiXml PARENT_SCOPE)
#set(Lutefisk3D_DEPENDENCIES ${Lutefisk3D_DEPENDENCIES} rapidjson PARENT_SCOPE)
set(Lutefisk3D_COMPONENT_SOURCES ${Lutefisk3D_COMPONENT_SOURCES} ${SOURCE} ${INCLUDES} PARENT_SCOPE)
