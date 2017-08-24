set(INCLUDES
    Application.h
    Engine.h
    EngineEvents.h
)
set(SOURCE
    Application.cpp
    Engine.cpp
    EngineEvents.cpp

)
add_library(Lutefisk3D_Engine STATIC ${SOURCE} ${INCLUDES})

target_include_directories(Lutefisk3D_Engine PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(Lutefisk3D_Engine PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>)
set_property(TARGET Lutefisk3D_Engine PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET Lutefisk3D_Engine PROPERTY INTERFACE_LINK_LIBRARIES
    Lutefisk3D_Core
)

target_link_libraries(Lutefisk3D_Engine jlsignal_IMP)
if(UNIT_TESTING)
#add_lutefisk_test(AttributeTests)
#add_lutefisk_test(ContextTests)
endif()
