set(INCLUDES
    Color.h
    Matrix3.h
    Matrix3x4.h
    Matrix4.h
    Quaternion.h
    Random.h
    Spline.h
    StringHash.h
    Vector2.h
    Vector3.h
    Vector4.h
)
set(SOURCE
    Color.cpp
    Matrix3.cpp
    Matrix3x4.cpp
    Matrix4.cpp
    Quaternion.cpp
    Random.cpp
    Spline.cpp
    Vector2.cpp
    Vector3.cpp
    Vector4.cpp
)


add_library(Lutefisk3D_Math OBJECT ${SOURCE} ${INCLUDES})
target_include_directories(Lutefisk3D_Math PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(Lutefisk3D_Math PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>)
set_property(TARGET Lutefisk3D_Math PROPERTY POSITION_INDEPENDENT_CODE ON)
