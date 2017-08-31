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
set_property(TARGET Lutefisk3D_Input PROPERTY INTERFACE_LINK_LIBRARIES
    SDL2_LIB
)
install(FILES ${INCLUDES} DESTINATION include/Lutefisk3D/Input )

if(UNIT_TESTING)
endif()
