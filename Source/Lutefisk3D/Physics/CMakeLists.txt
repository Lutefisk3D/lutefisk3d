set(INCLUDES
  CollisionShape.h
  Constraint.h
  PhysicsEvents.h
  PhysicsUtils.h
  PhysicsWorld.h
  RigidBody.h
)
set(SOURCE
  CollisionShape.cpp
  Constraint.cpp
  PhysicsWorld.cpp
  RigidBody.cpp
)
add_library(Lutefisk3D_Physics OBJECT ${SOURCE} ${INCLUDES})
add_dependencies(Lutefisk3D_Physics Bullet_LIB)
target_include_directories(Lutefisk3D_Physics PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(Lutefisk3D_Physics PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>)
set_property(TARGET Lutefisk3D_Physics PROPERTY POSITION_INDEPENDENT_CODE ON)

######################### Add component to the project's list
list(APPEND Lutefisk3D_components $<TARGET_OBJECTS:Lutefisk3D_Physics>)
SET(Lutefisk3D_components ${Lutefisk3D_components} PARENT_SCOPE)

if(UNIT_TESTING)
endif()
