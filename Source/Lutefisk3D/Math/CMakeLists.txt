set(INCLUDES
    AreaAllocator.h
    BoundingBox.h
    Color.h
    Frustum.h
    MathDefs.h
    Matrix2.h
    Matrix3.h
    Matrix3x4.h
    Matrix4.h
    Quaternion.h
    Plane.h
    Polyhedron.h
    Ray.h
    Rect.h
    Random.h
    Sphere.h
    Spline.h
    StringHash.h
    Vector2.h
    Vector3.h
    Vector4.h
)
set(SOURCE
    AreaAllocator.cpp
    BoundingBox.cpp
    Color.cpp
    Frustum.cpp
    Matrix2.cpp
    Matrix3.cpp
    Matrix3x4.cpp
    Matrix4.cpp
    Quaternion.cpp
    Random.cpp
    Plane.cpp
    Polyhedron.cpp
    Rect.cpp
    Ray.cpp
    Spline.cpp
    Sphere.cpp
    StringHash.cpp
    Vector2.cpp
    Vector3.cpp
    Vector4.cpp
)


add_library(Lutefisk3D_Math OBJECT ${SOURCE} ${INCLUDES})
target_include_directories(Lutefisk3D_Math PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(Lutefisk3D_Math PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>)
set_property(TARGET Lutefisk3D_Math PROPERTY POSITION_INDEPENDENT_CODE ON)
