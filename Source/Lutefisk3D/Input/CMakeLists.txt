set(INCLUDES
    Controls.h
    InputEvents.h
    Input.h
)
set(SOURCE
    Controls.cpp
    Input.cpp
)
add_library(Lutefisk3D_Input OBJECT ${SOURCE} ${INCLUDES})

target_include_directories(Lutefisk3D_Input PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(Lutefisk3D_Input PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>)
set_property(TARGET Lutefisk3D_Input PROPERTY POSITION_INDEPENDENT_CODE ON)

add_dependencies(Lutefisk3D_Input SDL2_LIB)

######################### Add component to the project's list
list(APPEND Lutefisk3D_components $<TARGET_OBJECTS:Lutefisk3D_Input>)
SET(Lutefisk3D_components ${Lutefisk3D_components} PARENT_SCOPE)

if(UNIT_TESTING)
#add_lutefisk_test(AttributeTests)
#add_lutefisk_test(ContextTests)
endif()
