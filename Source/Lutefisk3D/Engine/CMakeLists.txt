set(INCLUDES
    Application.h
    Engine.h
    EngineEvents.h
)
set(SOURCE
    Application.cpp
    Engine.cpp
)
add_library(Lutefisk3D_Engine OBJECT ${SOURCE} ${INCLUDES})
add_dependencies(Lutefisk3D_Engine LibCpuId)
target_include_directories(Lutefisk3D_Engine PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(Lutefisk3D_Engine PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>)
set_property(TARGET Lutefisk3D_Engine PROPERTY POSITION_INDEPENDENT_CODE ON)

if(UNIT_TESTING)
#add_lutefisk_test(AttributeTests)
#add_lutefisk_test(ContextTests)
endif()
