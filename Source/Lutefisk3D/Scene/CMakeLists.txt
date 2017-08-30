set(INCLUDES
    Animatable.h
    AnimationDefs.h
    Component.h
    LogicComponent.h
    Node.h
    ObjectAnimation.h
    ReplicationState.h
    Scene.h
    SceneEvents.h
    SceneResolver.h
    Serializable.h
    SmoothedTransform.h
    SplinePath.h
    UnknownComponent.h
    ValueAnimation.h
    ValueAnimationInfo.h
)
set(SOURCE
    Animatable.cpp
    Component.cpp
    Node.cpp
    LogicComponent.cpp
    ObjectAnimation.cpp
    Scene.cpp
    SceneResolver.cpp
    Serializable.cpp
    SmoothedTransform.cpp
    SplinePath.cpp
    UnknownComponent.cpp
    ValueAnimation.cpp
    ValueAnimationInfo.cpp
)


add_library(Lutefisk3D_Scene OBJECT ${SOURCE} ${INCLUDES})
target_include_directories(Lutefisk3D_Scene PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(Lutefisk3D_Scene PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>)
set_property(TARGET Lutefisk3D_Scene PROPERTY POSITION_INDEPENDENT_CODE ON)

if(UNIT_TESTING)
add_lutefisk_test(ValueAnimationTests)
endif()
