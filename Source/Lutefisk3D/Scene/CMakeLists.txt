set(INCLUDES
    Animatable.cpp
    ValueAnimation.h
    ValueAnimationInfo.h
)
set(SOURCE
    Animatable.h
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
