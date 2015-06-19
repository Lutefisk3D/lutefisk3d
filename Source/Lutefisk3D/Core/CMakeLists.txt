set(INCLUDES
    Attribute.h
    CoreEvents.h
    Condition.h
    Context.h
    Mutex.h
    Main.h
    Object.h
    ProcessUtils.h
    Profiler.h
    Thread.h
    Timer.h
    Variant.h
    WorkQueue.h
)
set(SOURCE
    Condition.cpp
    Context.cpp
    Mutex.cpp
    Object.cpp
    ProcessUtils.cpp
    Profiler.cpp
    Thread.cpp
    Timer.cpp
    WorkQueue.cpp
)

add_library(Lutefisk3D_Core OBJECT ${SOURCE} ${INCLUDES})
target_include_directories(Lutefisk3D_Core PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(Lutefisk3D_Core PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>)
set_property(TARGET Lutefisk3D_Core PROPERTY POSITION_INDEPENDENT_CODE ON)
