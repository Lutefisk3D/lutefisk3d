set(INCLUDES
    Deserializer.h
    File.h
    IOEvents.h
    Log.h
    Serializer.h
)
set(SOURCE
    Log.cpp
)

add_library(Lutefisk3D_IO OBJECT ${SOURCE} ${INCLUDES})
target_include_directories(Lutefisk3D_IO PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(Lutefisk3D_IO PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>)
set_property(TARGET Lutefisk3D_IO PROPERTY POSITION_INDEPENDENT_CODE ON)
