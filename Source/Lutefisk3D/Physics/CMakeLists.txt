list (APPEND BULLET_LIBRARIES BulletSoftBody)
list (APPEND BULLET_LIBRARIES BulletDynamics)
list (APPEND BULLET_LIBRARIES BulletCollision)
list (APPEND BULLET_LIBRARIES LinearMath)
INCLUDE_DIRECTORIES(ThirdParty/Bullet/src)
set(INCLUDES
    CollisionShape.h
    Constraint.h
    PhysicsEvents.h
    PhysicsUtils.h
    PhysicsWorld.h
    RaycastVehicle.h
    RigidBody.h
)
set(SOURCE
    CollisionShape.cpp
    Constraint.cpp
    PhysicsWorld.cpp
    RaycastVehicle.cpp
    RigidBody.cpp
)
#if(LUTEFISK3D_AS_DLLS)
#    add_library(Lutefisk3D_Physics SHARED ${SOURCE} ${INCLUDES}
#        $<TARGET_OBJECTS:StanHull>
#    )
#    target_link_libraries(Lutefisk3D_Physics Bullet_LIB)
#else()
    add_library(Lutefisk3D_Physics OBJECT ${SOURCE} ${INCLUDES}
    )
    set_property(TARGET Lutefisk3D_Physics APPEND PROPERTY INTERFACE_LINK_LIBRARIES
        ${BULLET_LIBRARIES} StanHull
    )
#endif()
add_dependencies(Lutefisk3D_Physics  ${BULLET_LIBRARIES})
target_include_directories(Lutefisk3D_Physics PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(Lutefisk3D_Physics PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>)
set_property(TARGET Lutefisk3D_Physics PROPERTY POSITION_INDEPENDENT_CODE ON)
set(ThirdParty_requisites ${ThirdParty_requisites} ${BULLET_LIBRARIES} StanHull PARENT_SCOPE)
install(FILES ${INCLUDES} DESTINATION include/Lutefisk3D/Physics )

if(UNIT_TESTING)
endif()
