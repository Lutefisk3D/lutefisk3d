set(INCLUDES
    AbstractFile.h

    Log.h
    Deserializer.h
    Serializer.h

    Compression.h

    File.h
    FileSystem.h
    FileWatcher.h
    IOEvents.h

    RWOpsWrapper.h
    PackageFile.h

    MemoryBuffer.h
    VectorBuffer.h
    IOEvents.h
)
set(SOURCE
    Log.cpp
    Serializer.cpp
    Deserializer.cpp

    Compression.cpp
    File.cpp
    FileSystem.cpp
    FileWatcher.cpp

    PackageFile.cpp

    MemoryBuffer.cpp
    VectorBuffer.cpp
    IOEvents.cpp
)

add_library(Lutefisk3D_IO STATIC ${SOURCE} ${INCLUDES} $<TARGET_OBJECTS:LZ4> )
add_dependencies(Lutefisk3D_IO LZ4)
#set_property(TARGET Lutefisk3D_IO PROPERTY INTERFACE_LINK_LIBRARIES LZ4)
set_property(TARGET Lutefisk3D_IO PROPERTY INTERFACE_LINK_LIBRARIES
    Lutefisk3D_Core
    Lutefisk3D_Engine # for console support
)
target_include_directories(Lutefisk3D_IO PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(Lutefisk3D_IO PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>)
set_property(TARGET Lutefisk3D_IO PROPERTY POSITION_INDEPENDENT_CODE ON)
