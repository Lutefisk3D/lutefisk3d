set(INCLUDES
    Connection.h
    #HttpRequest.h
    NetworkEvents.h
    Network.h
    NetworkPriority.h
    Protocol.h
)
set(SOURCE
    Connection.cpp
    #HttpRequest.cpp
    Network.cpp
    NetworkPriority.cpp
)
add_library(Lutefisk3D_Network OBJECT ${SOURCE} ${INCLUDES})

target_include_directories(Lutefisk3D_Network PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(Lutefisk3D_Network PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>)
target_compile_options(Lutefisk3D_Network PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_OPTIONS>)
set_property(TARGET Lutefisk3D_Network PROPERTY POSITION_INDEPENDENT_CODE ON)

add_dependencies(Lutefisk3D_Network SDL2_LIB)

######################### Add component to the project's list
list(APPEND Lutefisk3D_components $<TARGET_OBJECTS:Lutefisk3D_Network>)
SET(Lutefisk3D_components ${Lutefisk3D_components} PARENT_SCOPE)

if(UNIT_TESTING)
endif()
