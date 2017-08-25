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
add_library(Lutefisk3D_Physics STATIC ${SOURCE} ${INCLUDES}
    $<TARGET_OBJECTS:StanHull>
)
add_dependencies(Lutefisk3D_Physics Bullet_LIB)
target_include_directories(Lutefisk3D_Physics PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(Lutefisk3D_Physics PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>)
set_property(TARGET Lutefisk3D_Physics PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET Lutefisk3D_Physics APPEND PROPERTY INTERFACE_LINK_LIBRARIES
    Bullet_LIB
)

if(UNIT_TESTING)
endif()
