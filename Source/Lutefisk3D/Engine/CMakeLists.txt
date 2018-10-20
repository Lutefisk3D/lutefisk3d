set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/Application.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/EngineDefs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/EngineEvents.h
)
set(SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/Application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EngineEvents.cpp
)
if(LUTEFISK3D_PLUGINS)
	LIST(APPEND SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/PluginApplication.cpp)
	LIST(APPEND INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/PluginApplication.h)
	set(Lutefisk3D_LINK_LIBRARIES ${Lutefisk3D_LINK_LIBRARIES} cr PARENT_SCOPE)
endif()

add_subdirectory(jlsignal)

install(FILES ${INCLUDES} DESTINATION include/Lutefisk3D/Engine)
if(UNIT_TESTING)
#add_lutefisk_test(AttributeTests)
#add_lutefisk_test(ContextTests)
endif()
set(Lutefisk3D_LINK_LIBRARIES ${Lutefisk3D_LINK_LIBRARIES} PARENT_SCOPE)
set(Lutefisk3D_COMPONENT_SOURCES ${Lutefisk3D_COMPONENT_SOURCES} ${SOURCE} ${INCLUDES} PARENT_SCOPE)
