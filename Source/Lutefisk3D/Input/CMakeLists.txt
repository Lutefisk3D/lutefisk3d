set(INCLUDES
    Controls.h
    InputEvents.h
    Input.h
)
set(SOURCE
    Controls.cpp
    Input.cpp
)
add_library(Lutefisk3D_Input SHARED ${SOURCE} ${INCLUDES})
target_include_directories(Lutefisk3D_Input PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(Lutefisk3D_Input PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>)
set_property(TARGET Lutefisk3D_Input PROPERTY POSITION_INDEPENDENT_CODE ON)

target_link_libraries(Lutefisk3D_Input Lutefisk3D SDL2_LIB Threads::Threads ${CMAKE_DL_LIBS})
qt5_use_modules(Lutefisk3D_Input Core)

if(UNIT_TESTING)
#add_lutefisk_test(AttributeTests)
#add_lutefisk_test(ContextTests)
endif()
