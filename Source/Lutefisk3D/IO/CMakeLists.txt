set(INCLUDES
    Log.h
    Deserializer.h
    Serializer.h

    File.h
    FileSystem.h
    FileWatcher.h
    IOEvents.h

    PackageFile.h

    MemoryBuffer.h
    VectorBuffer.h
)
set(SOURCE
    Log.cpp
    Serializer.cpp
    Deserializer.cpp

    File.cpp
    FileSystem.cpp
    FileWatcher.cpp

    PackageFile.cpp

    MemoryBuffer.cpp
    VectorBuffer.cpp
)

add_library(Lutefisk3D_IO OBJECT ${SOURCE} ${INCLUDES})
add_dependencies(Lutefisk3D_IO LZ4)

target_include_directories(Lutefisk3D_IO PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(Lutefisk3D_IO PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>)
set_property(TARGET Lutefisk3D_IO PROPERTY POSITION_INDEPENDENT_CODE ON)
