set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/LAssert.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lutefisk3D.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Attribute.h
    ${CMAKE_CURRENT_SOURCE_DIR}/CoreEvents.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Condition.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Context.h
    ${CMAKE_CURRENT_SOURCE_DIR}/EventNameRegistrar.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Mutex.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Main.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Object.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ProcessUtils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Profiler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/StringHashRegister.h
    ${CMAKE_CURRENT_SOURCE_DIR}/StringUtils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Thread.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Timer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Variant.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WorkQueue.h
    ${CMAKE_CURRENT_SOURCE_DIR}/CoreDecls.h
)
set(SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/Attribute.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Condition.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Context.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Mutex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Object.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ProcessUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Profiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Thread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Timer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StringHashRegister.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StringUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WorkQueue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Variant.cpp
)
install(FILES ${INCLUDES} DESTINATION include/Lutefisk3D/Core )
if(UNIT_TESTING)
    add_lutefisk_test(AttributeTests)
    add_lutefisk_test(ContextTests)
endif()

target_sources(Lutefisk3D PRIVATE ${SOURCE} ${INCLUDES})
set(Lutefisk3D_LINK_LIBRARIES ${Lutefisk3D_LINK_LIBRARIES} PARENT_SCOPE) #EnTT
#target_link_libraries(Lutefisk3D PRIVATE EnTT)
if(LUTEFISK3D_PROFILING)
    set(Lutefisk3D_LINK_LIBRARIES ${Lutefisk3D_LINK_LIBRARIES} easy_profiler PARENT_SCOPE)
    #target_link_libraries(Lutefisk3D PRIVATE easy_profiler_IMP)
endif()

