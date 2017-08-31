set(INCLUDES
    LAssert.h
    Attribute.h
    CoreEvents.h
    Condition.h
    Context.h
    EventNameRegistrar.h
    EventProfiler.h
    Mutex.h
    Main.h
    Object.h
    ProcessUtils.h
    Profiler.h
    StringUtils.h
    Thread.h
    Timer.h
    Variant.h
    WorkQueue.h
)
set(SOURCE
    Condition.cpp
    Context.cpp
    EventProfiler.cpp
    Mutex.cpp
    Object.cpp
    ProcessUtils.cpp
    Profiler.cpp
    Thread.cpp
    Timer.cpp
    StringUtils.cpp
    WorkQueue.cpp
    Variant.cpp
)
add_library(Lutefisk3D_Core OBJECT ${SOURCE} ${INCLUDES})

target_include_directories(Lutefisk3D_Core PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(Lutefisk3D_Core PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>)
set_property(TARGET Lutefisk3D_Core PROPERTY POSITION_INDEPENDENT_CODE ON)
install(FILES ${INCLUDES} DESTINATION include/Lutefisk3D/Core )
if(UNIT_TESTING)
add_lutefisk_test(AttributeTests)
add_lutefisk_test(ContextTests)
endif()
