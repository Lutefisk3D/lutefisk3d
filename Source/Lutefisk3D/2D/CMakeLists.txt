set(INCLUDES
AnimatedSprite2D.h
AnimationSet2D.h
CollisionBox2D.h
CollisionChain2D.h
CollisionCircle2D.h
CollisionEdge2D.h
CollisionPolygon2D.h
CollisionShape2D.h
Constraint2D.h
ConstraintDistance2D.h
ConstraintFriction2D.h
ConstraintGear2D.h
ConstraintMotor2D.h
ConstraintMouse2D.h
ConstraintPrismatic2D.h
ConstraintPulley2D.h
ConstraintRevolute2D.h
ConstraintRope2D.h
ConstraintWeld2D.h
ConstraintWheel2D.h
Drawable2D.h
ParticleEffect2D.h
ParticleEmitter2D.h
PhysicsEvents2D.h
PhysicsUtils2D.h
PhysicsWorld2D.h
Renderer2D.h
RigidBody2D.h
Sprite2D.h
SpriterData2D.h
SpriterInstance2D.h
SpriteSheet2D.h
StaticSprite2D.h
TileMap2D.h
TileMapDefs2D.h
TileMapLayer2D.h
TmxFile2D.h
Urho2D.h
)
set(SOURCE
AnimatedSprite2D.cpp
AnimationSet2D.cpp
CollisionBox2D.cpp
CollisionChain2D.cpp
CollisionCircle2D.cpp
CollisionEdge2D.cpp
CollisionPolygon2D.cpp
CollisionShape2D.cpp
Constraint2D.cpp
ConstraintDistance2D.cpp
ConstraintFriction2D.cpp
ConstraintGear2D.cpp
ConstraintMotor2D.cpp
ConstraintMouse2D.cpp
ConstraintPrismatic2D.cpp
ConstraintPulley2D.cpp
ConstraintRevolute2D.cpp
ConstraintRope2D.cpp
ConstraintWeld2D.cpp
ConstraintWheel2D.cpp
Drawable2D.cpp
ParticleEffect2D.cpp
ParticleEmitter2D.cpp
PhysicsWorld2D.cpp
Renderer2D.cpp
RigidBody2D.cpp
Sprite2D.cpp
SpriterData2D.cpp
SpriterInstance2D.cpp
SpriteSheet2D.cpp
StaticSprite2D.cpp
TileMap2D.cpp
TileMapDefs2D.cpp
TileMapLayer2D.cpp
TmxFile2D.cpp
Urho2D.cpp
)

add_library(Lutefisk3D_2D OBJECT ${SOURCE} ${INCLUDES})
set_property(TARGET Lutefisk3D_2D APPEND PROPERTY INTERFACE_LINK_LIBRARIES PugiXml Box2D)
    target_include_directories(Lutefisk3D_2D PRIVATE
        $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:Qt5::Gui,INTERFACE_INCLUDE_DIRECTORIES>
    )
    target_compile_definitions(Lutefisk3D_2D PRIVATE
        $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>
        $<TARGET_PROPERTY:Qt5::Gui,INTERFACE_COMPILE_DEFINITIONS>
    )
add_dependencies(Lutefisk3D_2D PugiXml Box2D)
set(ThirdParty_requisites ${ThirdParty_requisites} PugiXml Box2D PARENT_SCOPE)
set_property(TARGET Lutefisk3D_2D PROPERTY POSITION_INDEPENDENT_CODE ON)


