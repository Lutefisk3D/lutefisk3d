set(INCLUDES
Allocator.h
ArrayPtr.h
Hash.h
HashMap.h
HashTable.h
Ptr.h
RefCounted.h
SmallOrig.h
SmallVector.h
Str.h
Swap.h

sherwood_map.hpp
)
set(SOURCE
    Allocator.cpp
    RefCounted.cpp
    Str.cpp
)

add_library(Lutefisk3D_Containers OBJECT ${SOURCE} ${INCLUDES})

target_include_directories(Lutefisk3D_Containers PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(Lutefisk3D_Containers PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>)
set_property(TARGET Lutefisk3D_Containers PROPERTY POSITION_INDEPENDENT_CODE ON)
